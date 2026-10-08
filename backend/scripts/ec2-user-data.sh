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

# Tools to compile the C++ server and the SQLite dev headers.
apt-get update -y
apt-get install -y build-essential cmake libsqlite3-dev git

# Get the website plus the backend code.
mkdir -p /opt/te
if [ ! -d /opt/te/.git ]; then
    git clone https://github.com/KeidranJr/taiwanese-enterprises /opt/te
else
    git -C /opt/te pull
fi

# Build the server.
cmake -S /opt/te/backend -B /opt/te/backend/build
cmake --build /opt/te/backend/build -j"$(nproc)"

# Install and start the systemd service (listens on port 80).
cp /opt/te/backend/te-backend.service /etc/systemd/system/te-backend.service
systemctl daemon-reload
systemctl enable te-backend
systemctl start te-backend

echo "Taiwanese Enterprises backend installed and started."
