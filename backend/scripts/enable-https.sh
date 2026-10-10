#!/bin/bash
# enable-https.sh — put taiwanenterprise.com on HTTPS with a free Let's
# Encrypt certificate, served through nginx as a reverse proxy.
#
# What it does:
#   1. Installs nginx + certbot.
#   2. Moves te_server to localhost:8080 via a systemd override (the main
#      unit file, including your Gmail secrets, is never touched).
#   3. nginx listens on 80 and proxies to the backend.
#   4. certbot gets a certificate for taiwanenterprise.com and
#      www.taiwanenterprise.com, turns on 443, and redirects 80 to 443.
#   5. Health-checks http and https.
#
# Run this AFTER the domain's DNS A records point at this box, or the
# certificate request will fail. Run with sudo.
# Idempotent: safe to re-run.
set -euo pipefail

DOMAIN="taiwanenterprise.com"
WWW="www.taiwanenterprise.com"
EMAIL="taiwanenterprisellc@gmail.com"
BACKEND_PORT=8080
OVERRIDE_DIR=/etc/systemd/system/te-backend.service.d

echo "== installing nginx and certbot =="
apt-get update -qq
DEBIAN_FRONTEND=noninteractive apt-get install -y -qq nginx certbot python3-certbot-nginx >/dev/null

echo "== moving te_server to localhost:$BACKEND_PORT (override, main unit untouched) =="
mkdir -p "$OVERRIDE_DIR"
cat > "$OVERRIDE_DIR/override.conf" <<EOF
[Service]
Environment=PORT=$BACKEND_PORT
EOF
systemctl daemon-reload
systemctl restart te-backend
sleep 3
systemctl is-active --quiet te-backend || { echo "ERROR: te-backend did not start on port $BACKEND_PORT"; exit 1; }

echo "== writing nginx reverse proxy config =="
cat > /etc/nginx/sites-available/te <<EOF
server {
    listen 80;
    listen [::]:80;
    server_name $DOMAIN $WWW;
    location / {
        proxy_pass http://127.0.0.1:$BACKEND_PORT;
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto \$scheme;
    }
}
EOF
ln -sf /etc/nginx/sites-available/te /etc/nginx/sites-enabled/te
rm -f /etc/nginx/sites-enabled/default
nginx -t
systemctl reload nginx

echo "== requesting Let's Encrypt certificate (DNS must already point here) =="
certbot --nginx --non-interactive --agree-tos --redirect --email "$EMAIL" -d "$DOMAIN" -d "$WWW"

echo "== health check =="
curl -s -o /dev/null -w "http:  %{http_code}\n" "http://$DOMAIN/"
curl -s -o /dev/null -w "https: %{http_code}\n" "https://$DOMAIN/"
curl -s -o /dev/null -w "www:   %{http_code}\n" "https://$WWW/"
curl -s "http://127.0.0.1:$BACKEND_PORT/api/services" | head -c 120; echo
echo OK
