#!/bin/bash
id opticon >/dev/null 2>&1 || {
  useradd -M -r -G opticon opticon
}
chgrp opticon /etc/opticon
chmod 750 /etc/opticon
