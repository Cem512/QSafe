@echo off
echo ========================================
echo QSafe Dev Dashboard - File Transfer
echo ========================================
echo.
echo Transferring to: cex@192.168.31.28
echo Password: orhint35
echo.
echo You will be prompted for password 9 times
echo (Press Ctrl+C to cancel)
echo.
pause

echo.
echo [1/9] Creating directory...
ssh cex@192.168.31.28 "mkdir -p /home/cex/dev-dashboard/templates"

echo [2/9] Transferring setup_pi.sh...
scp setup_pi.sh cex@192.168.31.28:/home/cex/dev-dashboard/

echo [3/9] Transferring dashboard_server.py...
scp dashboard_server.py cex@192.168.31.28:/home/cex/dev-dashboard/

echo [4/9] Transferring requirements.txt...
scp requirements.txt cex@192.168.31.28:/home/cex/dev-dashboard/

echo [5/9] Transferring README.md...
scp README.md cex@192.168.31.28:/home/cex/dev-dashboard/

echo [6/9] Transferring SETUP_INSTRUCTIONS.md...
scp SETUP_INSTRUCTIONS.md cex@192.168.31.28:/home/cex/dev-dashboard/

echo [7/9] Transferring FILES_SUMMARY.txt...
scp FILES_SUMMARY.txt cex@192.168.31.28:/home/cex/dev-dashboard/

echo [8/9] Transferring QUICK_REFERENCE.md...
scp QUICK_REFERENCE.md cex@192.168.31.28:/home/cex/dev-dashboard/

echo [9/9] Transferring dashboard.html...
scp templates\dashboard.html cex@192.168.31.28:/home/cex/dev-dashboard/templates/

echo.
echo [Final] Making setup script executable...
ssh cex@192.168.31.28 "chmod +x /home/cex/dev-dashboard/setup_pi.sh"

echo.
echo ========================================
echo Transfer Complete!
echo ========================================
echo.
echo Next steps:
echo 1. ssh cex@192.168.31.28
echo 2. cd /home/cex/dev-dashboard
echo 3. ./setup_pi.sh
echo.
pause
