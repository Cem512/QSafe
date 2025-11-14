"""
QSafe Development Dashboard Server
Real-time visualization of seismograph node data
"""

import json
import base64
import logging
import time
from datetime import datetime
from collections import deque
import numpy as np
from scipy import signal
import paho.mqtt.client as mqtt
from flask import Flask, render_template, jsonify
from flask_socketio import SocketIO, emit

# Configuration
MQTT_BROKER = "192.168.31.28"  # Local Raspberry Pi
MQTT_PORT = 1883
STREAM_BUFFER_SIZE = 2000  # Show last 10 seconds @ 200 Hz
STA_LTA_WINDOW = 1000  # 5 seconds @ 200 Hz

# Seismic frequency band filtering (match ESP32 configuration)
SAMPLE_RATE = 200  # Hz - streaming rate from ESP32
EARTHQUAKE_BAND_LOW = 0.5   # Hz - P-wave and S-wave lower bound
EARTHQUAKE_BAND_HIGH = 15.0  # Hz - P-wave and S-wave upper bound
FILTER_ORDER = 4  # Butterworth filter order

# Setup logging
logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Flask app
app = Flask(__name__)
app.config['SECRET_KEY'] = 'qsafe-dev-dashboard'
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')

# Data buffers (thread-safe deques)
stream_data = {
    'x': deque(maxlen=STREAM_BUFFER_SIZE),
    'y': deque(maxlen=STREAM_BUFFER_SIZE),
    'z': deque(maxlen=STREAM_BUFFER_SIZE),
    'timestamps': deque(maxlen=STREAM_BUFFER_SIZE)
}

node_stats = {
    'rms': 0.0,
    'kurtosis': 0.0,
    'peak_freq': 0.0,
    'sta_lta_ratio': 0.0,
    'orientation': {'roll': 0, 'pitch': 0},
    'node_id': 'Unknown',
    'last_update': None,
    'trigger_count': 0
}

trigger_events = deque(maxlen=50)  # Last 50 triggers

# Detection log - stores all threshold exceedances for manual verification
detection_log = deque(maxlen=200)  # Store last 200 detections

# Control limits tracking (Statistical Process Control)
control_limits = {
    'lta_history': deque(maxlen=1000),  # Store last 1000 LTA values for statistics
    'mean': 0.0,
    'std': 0.0,
    'ucl': 0.0,  # Upper Control Limit = mean + k*std
    'k_sigma': 2.0,  # Number of standard deviations (configurable: 1.5, 2.0, 2.5, 3.0, etc.)
    'adaptive_threshold': 3.0,  # Adaptive threshold for STA/LTA ratio
    # Self-learning parameters
    'learning_enabled': True,  # Enable automatic k-sigma adjustment
    'ratio_history': deque(maxlen=5000),  # Store STA/LTA ratios for learning
    'spike_count': 0,  # Count of values exceeding current threshold
    'total_samples': 0,  # Total samples processed
    'false_positive_rate': 0.0,  # Current false positive rate
    'target_fp_rate': 0.001,  # Target false positive rate (0.1% = 1 per 1000 samples)
    'last_adjustment': 0,  # Timestamp of last k-sigma adjustment
    'adjustment_interval': 300,  # Adjust k-sigma every 5 minutes (300 seconds)
    'last_detection_time': 0,  # Timestamp of last detection (for cooldown)
    'detection_cooldown': 5.0  # Minimum seconds between logged detections (reduces spam)
}

# Per-axis STA/LTA buffers
per_axis_data = {
    'x': {
        'buffer': deque(maxlen=STREAM_BUFFER_SIZE),
        'sta': 0.0,
        'lta': 0.0,
        'ratio': 0.0,
        'threshold': 3.0
    },
    'y': {
        'buffer': deque(maxlen=STREAM_BUFFER_SIZE),
        'sta': 0.0,
        'lta': 0.0,
        'ratio': 0.0,
        'threshold': 3.0
    },
    'z': {
        'buffer': deque(maxlen=STREAM_BUFFER_SIZE),
        'sta': 0.0,
        'lta': 0.0,
        'ratio': 0.0,
        'threshold': 3.0
    }
}

# Initialize Butterworth lowpass filter to remove high-frequency noise (>15 Hz)
# This preserves DC and low-frequency content while removing mechanical noise
# Different from ESP32's bandpass - we want to preserve steady-state energy for STA/LTA
nyquist = SAMPLE_RATE / 2.0
high = EARTHQUAKE_BAND_HIGH / nyquist
sos_lowpass = signal.butter(FILTER_ORDER, high, btype='low', output='sos')

logger.info(f"Lowpass filter initialized: 0-{EARTHQUAKE_BAND_HIGH} Hz (order {FILTER_ORDER})")


def apply_lowpass_filter(data):
    """
    Apply Butterworth lowpass filter to remove high-frequency noise (> 15 Hz)

    This removes:
    - High-frequency noise (> 15 Hz): HVAC, electrical, mechanical vibrations

    Preserves:
    - DC component (steady-state vibration energy)
    - Low frequencies (< 0.5 Hz): building resonance, thermal drift
    - Seismic frequencies (0.5-15 Hz): P-waves and S-waves

    This is different from ESP32's bandpass filter - we preserve more energy
    for better STA/LTA baseline estimation while still rejecting high-freq noise.

    Args:
        data: Array-like of acceleration values

    Returns:
        Filtered data array
    """
    if len(data) < 3 * FILTER_ORDER:  # Need minimum samples for filter stability
        return np.array(data)

    # Apply zero-phase filtering (forward + backward pass = no phase distortion)
    filtered = signal.sosfiltfilt(sos_lowpass, np.array(data))
    return filtered


def calculate_sta_lta(data, sta_window=100, lta_window=500):
    """
    Calculate STA/LTA ratio from acceleration data with lowpass filtering

    This applies 15 Hz lowpass filter before energy calculation to reject
    high-frequency noise (HVAC, electrical interference) while preserving
    steady-state energy for better baseline estimation.
    """
    if len(data) < lta_window:
        return {'sta': 0.0, 'lta': 0.0, 'ratio': 0.0}

    # Apply lowpass filter to remove high-frequency noise (> 15 Hz)
    arr = apply_lowpass_filter(data)

    # Calculate energy from filtered signal
    energy = arr ** 2

    # Calculate STA (last sta_window samples)
    sta = float(np.mean(energy[-sta_window:]))

    # Calculate LTA (last lta_window samples)
    lta = float(np.mean(energy[-lta_window:]))

    # Calculate ratio
    if lta < 1e-10:
        ratio = 0.0
    else:
        ratio = sta / lta

    return {'sta': sta, 'lta': lta, 'ratio': ratio}


def update_control_limits(lta_value):
    """
    Update adaptive control limits based on LTA history (SPC approach)

    This calculates the UCL (Upper Control Limit) from baseline statistics,
    then derives an adaptive STA/LTA ratio threshold from it.

    Adaptive Threshold Logic:
    - UCL = μ + k·σ (statistical upper bound for normal baseline noise)
    - Adaptive Threshold = UCL / μ (converts absolute energy to ratio)
    - This threshold adapts to environmental noise changes
    """
    control_limits['lta_history'].append(lta_value)

    # Need at least 100 samples to establish baseline
    if len(control_limits['lta_history']) < 100:
        return

    # Calculate mean and standard deviation of LTA (noise baseline)
    lta_array = np.array(list(control_limits['lta_history']))
    mean = float(np.mean(lta_array))
    std = float(np.std(lta_array))

    # Upper Control Limit: mean + k*sigma
    # k-sigma determines sensitivity:
    # - k=2.0: 95.4% of normal noise below UCL (more sensitive)
    # - k=3.0: 99.7% of normal noise below UCL (balanced)
    # - k=4.0: 99.99% of normal noise below UCL (less sensitive)
    k = control_limits['k_sigma']
    ucl = mean + k * std

    # Calculate adaptive threshold for STA/LTA ratio detection
    # This converts the absolute energy UCL to a relative ratio threshold
    # Example: If μ=244.76 mg² and UCL=263.43 mg², threshold = 263.43/244.76 = 1.076
    if mean > 1e-6:  # Avoid division by zero
        raw_threshold = ucl / mean

        # Apply floor to prevent over-sensitivity
        # Minimum threshold of 1.5 ensures we don't trigger on tiny variations
        # Maximum of 3.0 prevents being too strict in noisy environments
        adaptive_threshold = max(1.5, min(raw_threshold, 3.0))

        control_limits['adaptive_threshold'] = adaptive_threshold

        logger.debug(f"Adaptive threshold: UCL/μ = {ucl:.2f}/{mean:.2f} = {raw_threshold:.3f} → clamped to {adaptive_threshold:.3f}")
    else:
        # Fallback to conservative fixed threshold if baseline not established
        control_limits['adaptive_threshold'] = 3.0

    # Update control limits
    control_limits['mean'] = mean
    control_limits['std'] = std
    control_limits['ucl'] = ucl

    logger.debug(f"Control limits: μ={mean:.2f} mg², σ={std:.2f} mg², k={k:.2f}, UCL={ucl:.2f} mg², threshold={control_limits['adaptive_threshold']:.2f}")


def calculate_per_axis_sta_lta():
    """Calculate STA/LTA for each axis independently"""
    results = {}

    for axis in ['x', 'y', 'z']:
        axis_data = per_axis_data[axis]

        if len(axis_data['buffer']) < 500:
            results[axis] = {'sta': 0.0, 'lta': 0.0, 'ratio': 0.0}
            continue

        # Calculate STA/LTA for this axis
        sta_lta = calculate_sta_lta(axis_data['buffer'], sta_window=100, lta_window=500)

        # Update per-axis stats
        axis_data['sta'] = sta_lta['sta']
        axis_data['lta'] = sta_lta['lta']
        axis_data['ratio'] = sta_lta['ratio']

        results[axis] = sta_lta

    return results


def log_detection_event(sta_lta_ratio, sta_value, lta_value, per_axis_results=None):
    """
    Log a detection event when STA/LTA ratio exceeds adaptive threshold

    This creates a timestamped record with all relevant seismic parameters
    for later comparison with official earthquake reports.

    Args:
        sta_lta_ratio: The STA/LTA ratio that exceeded threshold
        sta_value: Short-term average energy
        lta_value: Long-term average energy
        per_axis_results: Per-axis STA/LTA breakdown (optional)
    """
    current_time = time.time()

    # Apply cooldown to prevent logging rapid successive detections
    # This groups events within 5 seconds into a single detection
    if current_time - control_limits['last_detection_time'] < control_limits['detection_cooldown']:
        return

    control_limits['last_detection_time'] = current_time

    # Create detection record
    detection = {
        'timestamp': datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
        'unix_time': current_time,
        'node_id': node_stats.get('node_id', 'Unknown'),
        'sta_lta_ratio': round(sta_lta_ratio, 3),
        'sta': round(sta_value, 2),
        'lta': round(lta_value, 2),
        'adaptive_threshold': round(control_limits['adaptive_threshold'], 3),
        'k_sigma': round(control_limits['k_sigma'], 2),
        'ucl': round(control_limits['ucl'], 2),
        'baseline_mean': round(control_limits['mean'], 2),
        'rms': round(node_stats.get('rms', 0.0), 2),
        'kurtosis': round(node_stats.get('kurtosis', 0.0), 2),
        'peak_freq': round(node_stats.get('peak_freq', 0.0), 2)
    }

    # Add per-axis breakdown if available
    if per_axis_results:
        detection['per_axis'] = {
            'x': round(per_axis_results.get('x', {}).get('ratio', 0.0), 3),
            'y': round(per_axis_results.get('y', {}).get('ratio', 0.0), 3),
            'z': round(per_axis_results.get('z', {}).get('ratio', 0.0), 3)
        }

    # Add to log
    detection_log.append(detection)

    # Log to console
    logger.warning(f"🚨 DETECTION LOGGED: {detection['timestamp']} | "
                  f"STA/LTA={sta_lta_ratio:.3f} (threshold={control_limits['adaptive_threshold']:.3f}) | "
                  f"RMS={detection['rms']:.2f} mg | Node={detection['node_id']}")

    # Emit to all connected web clients
    socketio.emit('detection_event', detection)


def learn_optimal_k_sigma(sta_lta_ratio, sta_value, lta_value, per_axis_results=None):
    """
    Self-learning algorithm to determine optimal k-sigma value
    Based on false positive rate when comparing STA/LTA ratio against adaptive threshold

    The algorithm:
    1. Tracks when STA/LTA ratio exceeds the adaptive threshold
    2. Logs the event with full seismic parameters for manual verification
    3. Calculates false positive rate (spikes / total samples)
    4. Adjusts k-sigma to maintain target false positive rate
    5. Higher k-sigma → higher threshold → fewer detections
    6. Lower k-sigma → lower threshold → more detections

    Args:
        sta_lta_ratio: The current STA/LTA ratio
        sta_value: Short-term average energy
        lta_value: Long-term average energy
        per_axis_results: Per-axis STA/LTA breakdown (optional)
    """
    if not control_limits['learning_enabled']:
        return

    # Track STA/LTA ratios for false positive analysis
    control_limits['ratio_history'].append(sta_lta_ratio)
    control_limits['total_samples'] += 1

    # Check if STA/LTA ratio exceeds current adaptive threshold
    # This represents a potential earthquake detection (or false positive)
    adaptive_threshold = control_limits['adaptive_threshold']
    if sta_lta_ratio > adaptive_threshold and adaptive_threshold > 0:
        control_limits['spike_count'] += 1
        logger.debug(f"Potential detection: STA/LTA={sta_lta_ratio:.3f} > threshold={adaptive_threshold:.3f}")

        # Log this detection event for manual verification
        log_detection_event(sta_lta_ratio, sta_value, lta_value, per_axis_results)

    # Need sufficient data before adjusting (at least 1000 samples)
    if control_limits['total_samples'] < 1000:
        return

    # Check if it's time to adjust k-sigma (every adjustment_interval seconds)
    current_time = time.time()
    if control_limits['last_adjustment'] == 0:
        control_limits['last_adjustment'] = current_time

    time_since_adjustment = current_time - control_limits['last_adjustment']
    if time_since_adjustment < control_limits['adjustment_interval']:
        return

    # Calculate current false positive rate
    fp_rate = control_limits['spike_count'] / control_limits['total_samples']
    control_limits['false_positive_rate'] = fp_rate

    # Target false positive rate (0.1% = 1 detection per 1000 samples)
    target_fp = control_limits['target_fp_rate']

    # Adjust k-sigma based on false positive rate
    current_k = control_limits['k_sigma']

    if fp_rate > target_fp * 2:  # Too many false positives
        # Increase k-sigma (make threshold stricter)
        new_k = min(current_k + 0.1, 5.0)  # Cap at 5.0
        logger.info(f"🔺 Learning: FP rate {fp_rate:.4f} too high (target {target_fp:.4f}), increasing k-sigma: {current_k:.2f} → {new_k:.2f}")
        control_limits['k_sigma'] = new_k

    elif fp_rate < target_fp * 0.5:  # Too few detections (might be too strict)
        # Decrease k-sigma (make threshold more sensitive)
        new_k = max(current_k - 0.1, 1.5)  # Lower limit at 1.5
        logger.info(f"🔻 Learning: FP rate {fp_rate:.4f} too low (target {target_fp:.4f}), decreasing k-sigma: {current_k:.2f} → {new_k:.2f}")
        control_limits['k_sigma'] = new_k

    else:
        logger.info(f"✓ Learning: FP rate {fp_rate:.4f} optimal (target {target_fp:.4f}), k-sigma stable at {current_k:.2f}")

    # Reset counters for next interval
    control_limits['spike_count'] = 0
    control_limits['total_samples'] = 0
    control_limits['last_adjustment'] = current_time


# MQTT Callbacks
def on_connect(client, userdata, flags, rc):
    """MQTT connection callback"""
    if rc == 0:
        logger.info("✓ Connected to MQTT broker")

        # Subscribe to all relevant topics
        client.subscribe("eew/stream/#", qos=0)  # High-frequency data
        client.subscribe("eew/heartbeat/#", qos=1)
        client.subscribe("eew/raw/#", qos=1)
        client.subscribe("eew/register", qos=1)

        logger.info("✓ Subscribed to topics")
    else:
        logger.error(f"✗ Connection failed with code {rc}")


def on_message(client, userdata, msg):
    """Handle incoming MQTT messages"""
    try:
        topic = msg.topic
        payload = json.loads(msg.payload.decode())

        logger.debug(f"Received message on topic: {topic}")

        if topic.startswith("eew/stream/"):
            logger.debug(f"Stream data: {payload}")
            handle_stream_data(payload)
        elif topic.startswith("eew/heartbeat/"):
            handle_heartbeat(payload)
        elif topic.startswith("eew/raw/"):
            handle_trigger(payload)
        elif topic == "eew/register":
            handle_registration(payload)

    except Exception as e:
        logger.error(f"Error processing message on topic {msg.topic}: {e}", exc_info=True)


def handle_stream_data(data):
    """Handle high-frequency streaming data (200 Hz)"""
    try:
        # Extract acceleration values
        x = data.get('x', 0.0)
        y = data.get('y', 0.0)
        z_raw = data.get('z', 0.0)
        timestamp = data.get('timestamp_ms', 0)

        # Apply gravity compensation to Z-axis (remove ~1000 mg bias)
        # Assumes node is roughly horizontal with Z pointing down
        z = z_raw + 1000.0  # Compensate for gravity

        # Add to buffers (store compensated Z)
        stream_data['x'].append(x)
        stream_data['y'].append(y)
        stream_data['z'].append(z)
        stream_data['timestamps'].append(timestamp)

        # Add to per-axis buffers for individual STA/LTA calculation
        per_axis_data['x']['buffer'].append(x)
        per_axis_data['y']['buffer'].append(y)
        per_axis_data['z']['buffer'].append(z)

        # Calculate magnitude (combined acceleration) - use compensated values
        magnitude = float(np.sqrt(x**2 + y**2 + z**2))

        # Calculate STA/LTA on magnitude (Approach 1: Combined)
        sta_lta_result = {'sta': 0.0, 'lta': 0.0, 'ratio': 0.0}
        if len(stream_data['x']) > 500:
            # Use combined magnitude for STA/LTA
            mag_buffer = deque(maxlen=STREAM_BUFFER_SIZE)
            for i in range(len(stream_data['x'])):
                mx = stream_data['x'][i]
                my = stream_data['y'][i]
                mz = stream_data['z'][i]
                mag_buffer.append(np.sqrt(mx**2 + my**2 + mz**2))

            sta_lta_result = calculate_sta_lta(mag_buffer)
            node_stats['sta'] = sta_lta_result['sta']
            node_stats['lta'] = sta_lta_result['lta']
            node_stats['sta_lta_ratio'] = sta_lta_result['ratio']

            # Update adaptive control limits (SPC approach)
            # This calculates UCL and derives adaptive threshold from it
            update_control_limits(sta_lta_result['lta'])

        # Calculate per-axis STA/LTA (Approach 2: Independent axes)
        per_axis_results = calculate_per_axis_sta_lta()

        # Self-learning k-sigma adjustment with detection logging
        # Pass all parameters for comprehensive event logging
        if len(stream_data['x']) >= 500:
            learn_optimal_k_sigma(
                sta_lta_result['ratio'],
                sta_lta_result['sta'],
                sta_lta_result['lta'],
                per_axis_results
            )

        # Emit to web clients (throttled - every 10th sample = 20 Hz update rate)
        if len(stream_data['x']) % 10 == 0:
            update_data = {
                'x': x,
                'y': y,
                'z': z,  # Send gravity-compensated Z
                'magnitude': magnitude,
                'timestamp': timestamp,
                # Combined magnitude STA/LTA
                'sta': sta_lta_result['sta'],
                'lta': sta_lta_result['lta'],
                'sta_lta_ratio': sta_lta_result['ratio'],
                # Control limits (SPC)
                'control_mean': control_limits['mean'],
                'control_std': control_limits['std'],
                'control_ucl': control_limits['ucl'],
                'adaptive_threshold': control_limits['adaptive_threshold'],
                'k_sigma': control_limits['k_sigma'],
                'false_positive_rate': control_limits['false_positive_rate'],
                'learning_enabled': control_limits['learning_enabled'],
                # Per-axis STA/LTA
                'per_axis': {
                    'x': per_axis_results.get('x', {'sta': 0.0, 'lta': 0.0, 'ratio': 0.0}),
                    'y': per_axis_results.get('y', {'sta': 0.0, 'lta': 0.0, 'ratio': 0.0}),
                    'z': per_axis_results.get('z', {'sta': 0.0, 'lta': 0.0, 'ratio': 0.0})
                }
            }
            logger.debug(f"Emitting stream_update: {update_data}")
            socketio.emit('stream_update', update_data)

    except Exception as e:
        logger.error(f"Error handling stream data: {e}", exc_info=True)


def handle_heartbeat(data):
    """Handle heartbeat messages with node statistics"""
    try:
        # Extract node ID from topic (will be set by caller) or use a default
        # For now, hardcode since we know we have Node_EFD0
        node_stats['node_id'] = 'Node_EFD0'  # TODO: Get from topic
        node_stats['last_update'] = datetime.now().isoformat()

        # ESP32 sends flat structure, extract directly
        node_stats['rms'] = data.get('rms_mg', 0.0)
        node_stats['kurtosis'] = 0.0  # Not in heartbeat, only in triggers
        node_stats['peak_freq'] = 0.0  # Not in heartbeat, only in triggers

        # Extract orientation directly from flat structure
        node_stats['orientation'] = {
            'roll': data.get('roll_deg', 0),
            'pitch': data.get('pitch_deg', 0)
        }

        # Extract triggers count
        node_stats['trigger_count'] = data.get('triggers_24h', 0)

        logger.debug(f"Heartbeat stats: {node_stats}")

        # Emit to web clients
        socketio.emit('stats_update', node_stats)

    except Exception as e:
        logger.error(f"Error handling heartbeat: {e}", exc_info=True)


def handle_trigger(data):
    """Handle trigger events"""
    try:
        node_id = data.get('node_id', 'Unknown')
        timestamp = data.get('timestamp', datetime.now().isoformat())
        trigger_info = data.get('trigger_info', {})

        event = {
            'timestamp': timestamp,
            'node_id': node_id,
            'rms': trigger_info.get('rms_mg', 0),
            'kurtosis': trigger_info.get('kurtosis', 0),
            'peak_freq': trigger_info.get('peak_freq_hz', 0)
        }

        trigger_events.append(event)
        node_stats['trigger_count'] += 1

        logger.info(f"Trigger from {node_id}: RMS={event['rms']:.1f}mg")

        # Emit to web clients
        socketio.emit('trigger_event', event)

    except Exception as e:
        logger.error(f"Error handling trigger: {e}")


def handle_registration(data):
    """Handle node registration"""
    node_id = data.get('node_id', 'Unknown')
    version = data.get('version', 'Unknown')
    logger.info(f"Node registered: {node_id} (v{version})")


# Flask Routes
@app.route('/')
def index():
    """Main dashboard page"""
    return render_template('dashboard.html')


@app.route('/api/stats')
def get_stats():
    """Get current node statistics"""
    return jsonify(node_stats)


@app.route('/api/triggers')
def get_triggers():
    """Get recent trigger events"""
    return jsonify(list(trigger_events))


@app.route('/api/stream')
def get_stream():
    """Get current stream buffer"""
    return jsonify({
        'x': list(stream_data['x']),
        'y': list(stream_data['y']),
        'z': list(stream_data['z']),
        'timestamps': list(stream_data['timestamps'])
    })


@app.route('/api/detections')
def get_detections():
    """Get detection event log for manual verification against earthquake reports"""
    return jsonify(list(detection_log))


# SocketIO Events
@socketio.on('connect')
def handle_connect():
    """Client connected"""
    logger.info("Web client connected")
    emit('initial_stats', node_stats)


@socketio.on('disconnect')
def handle_disconnect():
    """Client disconnected"""
    logger.info("Web client disconnected")


def main():
    """Main entry point"""
    logger.info("=" * 60)
    logger.info("QSafe Development Dashboard Server")
    logger.info("=" * 60)

    # Setup MQTT client
    mqtt_client = mqtt.Client(client_id="dev_dashboard")
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message

    # Connect to MQTT broker
    logger.info(f"Connecting to MQTT broker: {MQTT_BROKER}:{MQTT_PORT}")
    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        mqtt_client.loop_start()
    except Exception as e:
        logger.error(f"Failed to connect to MQTT broker: {e}")
        return

    # Start Flask web server
    logger.info("=" * 60)
    logger.info(f"Dashboard available at: http://192.168.31.28:5000")
    logger.info("=" * 60)

    socketio.run(app, host='0.0.0.0', port=5000, debug=False, allow_unsafe_werkzeug=True)


if __name__ == "__main__":
    main()
