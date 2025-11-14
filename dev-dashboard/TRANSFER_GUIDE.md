# Manual File Transfer Guide

Since automated SSH transfer requires additional tools, here's the easiest way to transfer files manually.

## Option 1: Using WinSCP (Recommended - GUI)

### Download WinSCP
1. Download from: https://winscp.net/eng/download.php
2. Install the program

### Transfer Files
1. Open WinSCP
2. Enter connection details:
   - File protocol: SFTP
   - Host name: `192.168.31.28`
   - Port number: `22`
   - User name: `cex`
   - Password: `orhint35`
3. Click "Login"
4. On left side (Windows), navigate to:
   ```
   C:\Users\cemfi\seismograph-system\dev-dashboard
   ```
5. On right side (Pi), create directory:
   ```
   /home/cex/dev-dashboard
   ```
6. Drag and drop all files from left to right:
   - setup_pi.sh
   - dashboard_server.py
   - requirements.txt
   - README.md
   - SETUP_INSTRUCTIONS.md
   - FILES_SUMMARY.txt
   - QUICK_REFERENCE.md
   - templates/ folder (entire folder)

7. Done! Close WinSCP

## Option 2: Using PowerShell SCP Commands

### Step 1: Open PowerShell as Administrator

### Step 2: Run these commands one by one

```powershell
# Create directory on Pi
ssh cex@192.168.31.28 "mkdir -p /home/cex/dev-dashboard/templates"
# Password: orhint35

# Transfer files
scp C:\Users\cemfi\seismograph-system\dev-dashboard\setup_pi.sh cex@192.168.31.28:/home/cex/dev-dashboard/
scp C:\Users\cemfi\seismograph-system\dev-dashboard\dashboard_server.py cex@192.168.31.28:/home/cex/dev-dashboard/
scp C:\Users\cemfi\seismograph-system\dev-dashboard\requirements.txt cex@192.168.31.28:/home/cex/dev-dashboard/
scp C:\Users\cemfi\seismograph-system\dev-dashboard\README.md cex@192.168.31.28:/home/cex/dev-dashboard/
scp C:\Users\cemfi\seismograph-system\dev-dashboard\SETUP_INSTRUCTIONS.md cex@192.168.31.28:/home/cex/dev-dashboard/
scp C:\Users\cemfi\seismograph-system\dev-dashboard\FILES_SUMMARY.txt cex@192.168.31.28:/home/cex/dev-dashboard/
scp C:\Users\cemfi\seismograph-system\dev-dashboard\QUICK_REFERENCE.md cex@192.168.31.28:/home/cex/dev-dashboard/
scp C:\Users\cemfi\seismograph-system\dev-dashboard\templates\dashboard.html cex@192.168.31.28:/home/cex/dev-dashboard/templates/

# Make setup script executable
ssh cex@192.168.31.28 "chmod +x /home/cex/dev-dashboard/setup_pi.sh"
```

You'll need to enter the password `orhint35` for each command.

## Option 3: Using USB Drive

1. Copy the entire `dev-dashboard` folder to a USB drive
2. Plug USB drive into Raspberry Pi
3. On Pi terminal:
   ```bash
   # Mount USB (if not auto-mounted)
   lsblk  # Find USB device (usually /dev/sda1)
   sudo mount /dev/sda1 /mnt

   # Copy files
   cp -r /mnt/dev-dashboard /home/cex/

   # Make script executable
   chmod +x /home/cex/dev-dashboard/setup_pi.sh

   # Unmount USB
   sudo umount /mnt
   ```

## After Transfer

SSH to Pi and verify files:
```bash
ssh cex@192.168.31.28
# Password: orhint35

ls -la /home/cex/dev-dashboard
```

You should see:
```
-rwxr-xr-x 1 cex cex  2345 Nov 14 10:00 setup_pi.sh
-rw-r--r-- 1 cex cex  7890 Nov 14 10:00 dashboard_server.py
-rw-r--r-- 1 cex cex   234 Nov 14 10:00 requirements.txt
drwxr-xr-x 2 cex cex  4096 Nov 14 10:00 templates
...
```

Then proceed with installation:
```bash
cd /home/cex/dev-dashboard
./setup_pi.sh
```
