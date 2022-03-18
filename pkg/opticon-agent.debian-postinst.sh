#!/bin/bash
id opticon >/dev/null 2>&1 || {
  useradd -M -r -U opticon
}
chgrp opticon /etc/opticon
chmod 750 /etc/opticon
chgrp opticon /run/opticon
chmod 770 /run/opticon
chgrp opticon /usr/bin/opticon-helper
chmod 4750 /usr/bin/opticon-helper
systemctl is-active --quiet opticon-agent && systemctl restart opticon-agent
