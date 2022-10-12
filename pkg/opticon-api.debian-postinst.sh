#!/bin/bash
id opticon >/dev/null 2>&1 || {
  useradd -M -r -U opticon
}
chgrp opticon /etc/opticon
chmod 750 /etc/opticon
systemctl is-active --quiet opticon-api
if [ $? = 0 ]; then
  systemctl restart opticon-api
fi
exit 0

