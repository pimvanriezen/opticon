#!/bin/bash
id opticon >/dev/null 2>&1 || {
  useradd -M -r -U opticon
}
chgrp opticon /etc/opticon
chmod 750 /etc/opticon
chown opticon /var/opticon/db
chown opticon /var/log/opticon
chgrp opticon /var/opticon/db
chgrp opticon /var/log/opticon
