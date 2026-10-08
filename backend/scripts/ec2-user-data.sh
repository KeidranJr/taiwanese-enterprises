#!/bin/bash
# EC2 user data script for the Taiwanese Enterprises LLC website.
# Paste this into the "User data" box when launching an Ubuntu 22.04
# or 24.04 instance. It runs once as root on first boot.
#
# IMPORTANT: the EC2 security group for the instance must allow
# inbound HTTP traffic on port 80, or nobody can reach the site.
# In the AWS console: EC2 > Security Groups > Inbound rules > Add rule >
# Type HTTP, Port 80, Source 0.0.0.0/0.

set -e
exec > /var/log/te-install.log 2>&1

echo "=== TE install starting ==="

# t3.micro only has 1GB RAM and compiling Crow needs more.
# Add 2GB of swap so the build does not run out of memory.
if [ ! -f /swapfile ]; then
    fallocate -l 2G /swapfile
    chmod 600 /swapfile
    mkswap /swapfile
    swapon /swapfile
    echo "/swapfile none swap sw 0 0" >> /etc/fstab
fi

# Tools to compile the C++ server, plus SQLite and libcurl dev headers.
apt-get update -y
apt-get install -y build-essential cmake libsqlite3-dev libcurl4-openssl-dev git

# Get the website plus the backend code.
mkdir -p /opt/te
if [ ! -d /opt/te/.git ]; then
    git clone https://github.com/KeidranJr/taiwanese-enterprises /opt/te
else
    git -C /opt/te pull
fi

# Build the server. -j1 on purpose: t3.micro cannot handle
# parallel Crow compiles without running out of memory.
cmake -S /opt/te/backend -B /opt/te/backend/build
cmake --build /opt/te/backend/build -j1

# Install and start the systemd service (listens on port 80).
cp /opt/te/backend/te-backend.service /etc/systemd/system/te-backend.service
systemctl daemon-reload
systemctl enable te-backend
systemctl start te-backend

echo "=== TE install finished ==="
systemctl status te-backend --no-pager || true
