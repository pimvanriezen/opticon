Opticon
=======

Opticon is a lightweight multi-tenant monitoring system, a way of
keeping tabs on what your servers are doing for purposes of monitoring,
performance analysis and incident forensics. 

The Opticon server agent uses *push messages* to send
updates to the central collector, allowing important performance data to keep
flowing even under adversial conditions, where traditional pollers tend to fail.

Components
----------

An installation of all opticon components will leave you with the following
binaries and path elements:

| Component                      | Function                                           |
| ------------------------------ | -------------------------------------------------- |
| /usr/sbin/opticon-agent        | The data probe that should run on monitored hosts. |
| /usr/sbin/opticon-collector    | The server component that gathers metering data.   |
| /usr/sbin/opticon-api          | The API server.                                    |
| /usr/bin/opticon               | The command line client to the API server.         |
| /etc/opticon                   | Directory for configuration.                       |
| /var/lib/opticon               | Directory for plugins.                             |
| /var/opticon/db                | Directory for the database.                        |

Configuring opticon-collector
-----------------------------

There are two levels of configuration at play for the collector daemon. The
first level is its configuration file, which sets up some of the basics, and
defines the default set of meters, and alert levels. The second level of
configuration is the tenant database. The latter has to be configured through
the opticon cli, so we’ll get to that after setting up the API server.

### The configuration file

The collector has a very simple base configuration in
`/etc/opticon/opticon-collector.conf`, only dealing with system resources:

```
network {
    port: 1047
    address: *
}
database {
    path: "/var/opticon/db"
}
```

Additionally, system-wide custom meters and watchers can be configured in
`/etc/opticon/opticon-meter.conf`, like this:

```
# Custom meter and watcher definitions
"pcpu" {
    type: frac
    description: "CPU Usage"
    unit: "%"
    warning { cmp: gt, val: 30.0, weight: 1.0 }
    alert { cmp: gt, val: 50.0, weight: 1.0 }
}
"hostname" {
    type: string
    description: "Hostname"
}
```

The configuration files are in a ’sloppy’ JSON format. You can use strict JSON
formatting, but you can also leave out any colons, commas, or quotes around
strings that have no whitespace or odd characters.

Configuring opticon-api
-----------------------

The API server keeps its configuration in `/etc/opticon/opticon-api.conf`. This
is how it typically looks:

```
network {
    port: 8888
}
auth {
    admin_token: a666ed1e-24dc-4533-acab-1efb2bb55081
    admin_host: 127.0.0.1
    auth_method: internal
}
database {
    path: "/var/opticon/db"
}
```

Generate a random UUID for the `admin_token` setting. Requests with this token
coming from the IP address specified by `admin_host` will be granted
administrative privileges.

The API server will also reference `/etc/opticon/opticon-meter.conf`, so it can
inform users of the actual defaults active.

Configuring the opticon client
------------------------------

The client gets its configuration from both `/etc/opticon/opticon-cli.conf` and
`$HOME/.opticonrc` (the latter having precedence, but both files are parsed and
merged). First let’s configure the global configuration file with the endpoint:

```
endpoints {
  opticon: "http://127.0.0.1:8888/"
}
```

In `.opticonrc` you can configure the `admin_token` as it was configured in the
api configuration:

```
defaults {
  admin_token: a666ed1e-24dc-4533-acab-1efb2bb55081
}
```

This is the `.opticonrc` you should set up for the root account, or another user
with an administrative role.

Managing the tenant database
----------------------------

Now that the collector and API-server are active, and the client knows how to
talk to them, the admin API can be used to add tenants to the database. Use the
following command to create a new tenant:

```Apex
$ opticon tenant-create --name "Acme"
Tenant created:
------------------------------------------------------------------
     Name: Acme
     UUID: dbe7c559-297e-e65b-9eca-fc96037c67e2
  AES Key: nwKT5sfGa+OlYHwa7rZZ7WQaMsAIEWKQii0iuSUPfG0=
------------------------------------------------------------------
```

The tool will spit out the UUID for the newly created tenant, as well as the
tenant AES256 key to be used in the configuration of this tenant’s
*opticon-agent* instances.

If you want to create a tenant with a predefined UUID, you can use the
`--tenant` command line flag:

```Apex
$ opticon tenant-create --name "Acme" --tenant 0296d893-8187-4f44-a31b-bf3b4c19fc10
```

This will be followed by the same information as the first example. Note that
creating tenants manually in a Keystone-enabled setup is going to be a bit
pointless. Users authenticated with a valid keystone token are allowed to create
the tenant record for tenants they have access to, so the entire set-up should
be self service.

### Getting an overview of tenants

If accessed through the admin API, the `tenant-list` sub-command will show
information about all tenants on the system:

```Apex
$ opticon tenant-list
UUID                                 Hosts  Name
--------------------------------------------------------------------------------
001b7153-4f4b-4f1c-b281-cc06b134f98f     2  compute-pim
0296d893-8187-4f44-a31b-bf3b4c19fc10     0  Acme
6c0606c4-97e6-37dc-14fc-b7c1c61effef     0  compute-demo
--------------------------------------------------------------------------------
```

The same command issued to the regular API will restrict this list to tenants
accessible to the user.

### Deleting a tenant

To get rid of a tenant (and reclaim all associated storage), use the
`tenant-delete` sub-command:

```Apex
$ opticon tenant-delete --tenant 0296d893-8187-4f44-a31b-bf3b4c19fc10
```

that should teach them. Users authenticated through Keystone are allowed to
clean up their own tenants, but not those of others, for reasons that should be
obvious.

### Creating a user account for the API/GUI

When opticon-api is not set up to integrate with an external authentication
provider, you can use the `user-create` sub-command, e.g.:
```Apex
$ opticon --tenant 0296d893-8187-4f44-a31b-bf3b4c19fc10 user-create --username john@acme.com
```

The command will ask for a password. Use the `--admin` flag to create a user
with admin privileges.

Configuring opticon-agent
-------------------------

With a tenantid and access key in hand, you can now go around and install
opticon-agent on servers that you would like to monitor. The agent reads its
configuration from `/etc/opticon/opticon-agent.conf`. First let’s take a look at
a rather straightforward configuration:

```
collector {
    address: 192.168.1.1
    port: 1047
    key: "nwKT5sfGa+OlYHwa7rZZ7WQaMsAIEWKQii0iuSUPfG0="
    tenant: "001b71534f4b4f1cb281cc06b134f98f"
}
probes {
}
```

The `collector` section tells the agent how to reach the opticon-collector, and
how to identify itself. If the server runs in an OpenStack cloud, you can change
the `config` setting from `manual` to `cloud-init`. This will tell the agent to
get the connection information out of the OpenStack metadata service, instead.
It will try to read the following metadata fields: `opticon_collector_address`,
`opticon_collector_port`, `opticon_tenant_key`, `opticon_tenant_id`, and
`opticon_host_id`.

The `probes` section defines any custom probes on top of the default set
collected by the agent, as well as any customisations for these existing
probes.

Accessing opticon as a user
---------------------------

After you used the admin API to create a tenant, you should be able to access
the rest of the functionality from any machine running an opticon client. 

The local `.opticonrc` shouldn't have an `admin_token`, but it’s possible to add
some convenience to the workflow by picking a default tenant for commands; most
users are likely to work with a single tenant and can do fine without typing
`--tenant`, and a huge UUID after each command by adding this to the opticon-cli
config:

```
defaults {
  tenant: 001b7153-4f4b-4f1c-b281-cc06b134f98f
}
```

With everything in place, calling opticon for the first time will now prompt you
for Keystone login credentials:

```Apex
$ opticon tenant-list
% Login required

  Username........: pi
  Password........: 

UUID                                 Hosts  Name
--------------------------------------------------------------------------------
001b7153-4f4b-4f1c-b281-cc06b134f98f     2  compute-pim
--------------------------------------------------------------------------------
```

The next time you issue a request, the client will use a cached version of the
token it acquired with your username and password. If you’re having issues with
your key, you can remove the cache file manually, it is stored in
`$HOME/.opticon-token-cache`.

Since this account only has one tenant, for the rest of this documentation, we
will assume that this tenant-id is specified in `.opticonrc`. In situations
where you have to deal with multiple tenants, the `--tenant` flag can added to
any command to indicate a specific tenant.

### Manipulating tenant metadata

Every tenant object in the opticon database has freeform metadata. Some of it is
used internally, like the tenant AES key. Use the `tenant-get-metadata`
sub-command to view a tenant’s metadata in JSON format:

```
$ opticon tenant-get-metadata
{
    "metadata": {
    }
}
```

You can add keys to the metadata, or change the value of existing keys, by using
the obviously named `tenant-set-metadata` sub-command:

```Apex
$ opticon tenant-set-metadata sleep optional
$ opticon tenant-get-metadata
{
    "metadata": {
        "sleep": "optional"
    }
}
```

Tenants also have a storage quota. Non-admin users can get information about
their current quota and usage metrics using the `tenant-get-quota` sub-command.
Admin users can change a tenant's storage quota using `tenant-set-quota`:

```Apex
$ opticon tenant-get-quota 
Tenant storage quota: 64 MB
Current usage: 90.62 %
```

The opticon collector will remove all host data for the oldest date recorded
when a tenant's quota usage reaches 100%.

### Navigating hosts

To get an overview of the hosts being monitored by the system, use the
`host-list` sub-command:

```Apex
$ opticon host-list
UUID                                    Size First record      Last record
--------------------------------------------------------------------------------
0d19d114-55c8-4077-9cab-348579c70612    5 MB 2014-09-24 13:57  2014-10-02 20:20
2b331038-aac4-4d8b-a7cd-5271b603bd1e    5 MB 2014-09-24 16:14  2014-10-02 20:20
--------------------------------------------------------------------------------
```

The times provided throughout opticon are always normalized to UTC. You can also
get a nice overview of the state your hosts using `host-overview`:

```
$ opticon host-overview
jones:opticon pi$ opticon host-overview 
Name                            Status     Load  Net i/o      CPU
--------------------------------------------------------------------------------
jones.local                     ALERT      0.95        6  15.36 % -[##        ]+
Jander.local                    OK         1.34        7   4.91 % -[          ]+
--------------------------------------------------------------------------------
```

Use the `host-show` sub-command to get the latest record available for a host.
You can use a host's uuid, or its hostname (if it is unique) to get at it:

```Apex
$ opticon host-show --host Jander.local
---( HOST )---------------------------------------------------------------------
UUID..............: 2b331038-aac4-4d8b-a7cd-5271b603bd1e
Hostname..........: Jander.local
Address...........: ::ffff:92.108.228.195
Status............: OK
Problems..........: 
Uptime............: 7 days, 11:22:58
OS/Hardware.......: Darwin 13.4.0 (x86_64)
Distribution......: OS X 10.9.5 (13F34)
---( RESOURCES )----------------------------------------------------------------
Processes.........: 167 (2 running, 2 stuck)
Load Average......:   1.33 /   1.61 /   1.57
CPU...............:   4.91 %                         -[#                     ]+
Available RAM.....: 16375.00 MB
Free RAM..........: 14947.00 MB
Network in/out....: 5 Kb/s (3 pps) / 2 Kb/s (3 pps)
Disk i/o..........: 0 rdops / 0 wrops
---( PROCESS LIST )-------------------------------------------------------------
USER                PID       CPU       MEM NAME 
pi                  561    9.00 %    0.00 % UA Mixer Engine 
root                  0    7.00 %    0.00 % kernel_task 
pi                  467    5.79 %    0.00 % Pianoteq 5 
pi                  224    5.69 %    0.00 % EuControl 
_coreaudiod         419    3.59 %    0.00 % coreaudiod 
root                122    1.69 %    0.00 % SGProtocolServi 
pi                 1104    0.79 %    0.00 % Console 
pi                  517    0.49 %    0.00 % MIDIServer 
root                  1    0.09 %    0.00 % launchd 
_locationd          102    0.09 %    0.00 % locationd 
---( STORAGE )------------------------------------------------------------------
DEVICE                 SIZE FS         USED MOUNTPOINT 
/dev/disk0s2      464.84 GB hfs     57.00 % / 
/dev/disk1s2      931.19 GB hfs     20.00 % /Volumes/Audio 
/dev/disk7       5588.40 GB hfs     49.00 % /Volumes/Oodle Nova 
/dev/disk6       5588.40 GB hfs     31.00 % /Volumes/Storage 
--------------------------------------------------------------------------------
```

Customizing alerts
------------------

The opticon-collector processes metering samples every minute, and uses a list
of *watchers* to determine whether there are any problems. A watcher is a
setting for a specific meter that compares it with a defined value, and uses the
outcome to attribute a level of ‘badness’ to a host.

The software ships with a default set of watchers that is hopefully useful for
most cases. You can look at the current situation by issuing the `watcher-list`
sub-command:

```Apex
$ opticon watcher-list
From     Meter        Trigger   Match                  Value             Weight
--------------------------------------------------------------------------------
default  df/pused     warning   gt                     90.00                1.0
default  df/pused     alert     gt                     95.00                1.0
default  df/pused     critical  gt                     99.00                1.0
default  pcpu         warning   gt                     70.00                1.0
default  pcpu         alert     gt                     90.00                1.0
default  pcpu         critical  gt                     99.00                1.0
default  loadavg      warning   gt                     10.00                1.0
default  loadavg      alert     gt                     20.00                1.0
default  loadavg      critical  gt                     50.00                1.0
default  proc/stuck   warning   gt                         6                1.0
default  proc/stuck   alert     gt                        10                1.0
default  proc/stuck   critical  gt                        20                1.0
default  net/in_kbs   warning   gt                     40000                0.5
default  net/in_pps   warning   gt                     10000                1.0
default  net/in_pps   alert     gt                     50000                1.0
default  net/in_pps   critical  gt                    100000                1.0
default  net/out_pps  warning   gt                     10000                1.0
default  net/out_pps  alert     gt                     50000                1.0
default  net/out_pps  critical  gt                    100000                1.0
default  mem/free     warning   lt                     65536                0.5
default  mem/free     alert     lt                     32768                1.0
default  mem/free     critical  lt                      4096                1.0
--------------------------------------------------------------------------------
```

You can change the settings for a watcher, by using the `watcher-set`
sub-command:

```Apex
$ opticon watcher-set --meter pcpu --level warning --value 40
$ opticon watcher-list
From     Meter        Trigger   Match                  Value             Weight
--------------------------------------------------------------------------------
default  df/pused     warning   gt                     90.00                1.0
default  df/pused     alert     gt                     95.00                1.0
default  df/pused     critical  gt                     99.00                1.0
tenant   pcpu         warning   gt                     40.00                1.0
default  pcpu         alert     gt                     50.00                1.0
default  pcpu         critical  gt                     99.00                1.0
...
```

The `weight` value for a watcher determines how fast it should cause the alert
level to rise. If you set it lower, the time it takes for an over-threshold
value to get to the various alert stages. To get rid of any customizations,
issue the `watcher-delete` command with the proper `--meter` provided.

If you want to view or change watchers only for a specific host, specify the
host with the `--host` flag.

Creating custom meters
----------------------

Opticon allows you to fully customize the meters that are being transmitted
between the agent and the collector. Before we start designing our own custom
meter, first some bits about the data model. By running the client with the
`--json` flag, you can get an idea of the structure:

```Apex
$ opticon host-show --host 2b331038-aac4-4d8b-a7cd-5271b603bd1e --json
{
    "agent": {
        "ip": "fe80::8a53:95ff:fe32:557"
    },
    "hostname": "Jander.local",
    "loadavg": [
        1.383000,
        1.391000,
        1.422000
    ],
    "os": {
        "kernel": "Darwin",
        "version": "13.3.0",
        "arch": "x86_64"
    },
    "df": [
        {
            "device": "/dev/disk0s2",
            "size": 475992,
            "pused": 57.000000,
            "mount": "/",
            "fs": "hfs"
        },
        {
            "device": "/dev/disk1s2",
            "size": 953541,
            "pused": 20.000000,
            "mount": "/Volumes/Audio",
            "fs": "hfs"
        }
    },
...
```

This looks deceptively structured. But opticon data is not free form. The limits
imposed by UDP packet sizes necessitate some engineering compromises. To keep
bandwidth from exploding, the underlying data is actually implemented as a flat
list of values and arrays, with dictionary/hashes at the JSON level being purely
an illusion limited to two levels. The data above, represented internally, would
look like this:

```javascript
{
    "agent/ip": "fe80::8a53:95ff:fe32:557",
    "hostname": "Jander.local",
    "loadavg": [1.383, 1.391, 1.422],
    "os/kernel": "Darwin",
    "os/version": "13.3.0",
    "os/arch": "x86_64",
    "df/device": ["/dev/disk0s2", "/dev/disk1s2"],
    "df/size": [475992, 953541],
    "df/pused": [57.000, 20.000],
    "df/mount": ["/", "/Volumes/Audio"],
    "df/fs": ["hfs", "hfs"]
}
```

This representation, as noted, allows for a limited set of JSON constructs
involving dictionaries:

```Apex
"key": "string" # obviously

"key": 18372 # unsigned 63-bit integer

"key": 1.3 # fractional number

# grouped values
"key": {
    "key": "value",
    "other": 42
}

"key": ["string", "string"] # array of strings

"key": [13, 37, 42] # array of unsigned integers

"key": [1.0, 2.07, 3.14] # array of fractional numbers

#table
"key": [
    {"key1": "valueA", "key2": 42},
    {"key1": "valueB", "key2": 64}
]
```

A further limitation is length of the keys. The maximum size of a key name is
11. If you’re at a second level, the sum of the length of the key name and its
parent key name cannot be larger than 10 (one is lost for the ‘/‘). There’s also
a very limited character set to choose from for keys:

```Apex
a b c d e f g h i j k l m n o p q r s t u v w x y z . - _ / @
```

In addition, arrays are limited in size to a maximum of 29 items.

Writing a custom probe
----------------------

With all this fresh knowledge in hand, let’s try to write a real world probe.
For this example, we will query the battery level of a MacBook and start
transmitting this as a meter.

The battery level can be queried from the command line using the `pmset`
utility. Its output looks like this:

```Apex
$ pmset -g batt
Now drawing from 'AC Power'
 -InternalBattery-0     97%; charged; 0:00 remaining
```

We’ll write an ugly script to turn that information into JSON:

```Apex
$ cat /usr/local/scripts/getpower.sh
#!/bin/sh
charge=$(pmset -g batt | grep InternalBattery | cut -f2 | cut -f1 -d'%')
source=$(pmset -g batt | head -1 | cut -f2 -d "'" | sed -e "s/ Power//")
printf '{"power":{"level":%.2f,"src":"%s"}}\n' "$charge" "$source"

$ /usr/local/scripts/getpower.sh
{"power":{"level":96.00,"src":"AC"}}
```

Now we can add this script to the `probes` section of `opticon-agent.conf`:

```
probes {
    power {
        type: exec
        call: /usr/local/scripts/getpower.sh
        interval: 60
    }
    ...
```

After restarting the agent, and waiting for the next minute mark to pass, and
collector to write out its data, the value should be visible in the `host-show`
JSON output:

```
    "mem": {
        "total": 8386560,
        "free": 6928384
    },
    "uptime": 184495,
    "power": {
        "level": 96.000000,
        "src": "AC"
    },
    "status": "WARN",
```

Et voila, an extra meter was born.

### Configuring the meter for the tenant

If you want your meter to show up less cryptically, you should add information
the meter to the tenant’s database using the command line tool:

```Apex
$ opticon meter-create --meter power/level --type frac --description "Battery Level" --unit "%"
$ opticon meter-create --meter power/src --type string --description "Power Source"
$ opticon meter-list
From     Meter        Type      Unit    Description
--------------------------------------------------------------------------------
default  agent/ip     string            Remote IP Address
default  os/kernel    string            Version
default  os/arch      string            CPU Architecture
default  df/device    string            Device
default  df/size      integer   KB      Size
default  df/pused     frac      %       In Use
default  uptime       integer   s       Uptime
default  top/pid      integer           PID
default  top/name     string            Name
default  top/pcpu     frac      %       CPU Usage
default  top/pmem     frac      %       RAM Usage
default  pcpu         frac      %       CPU Usage
default  loadavg      frac              Load Average
default  proc/total   integer           Total processes
default  proc/run     integer           Running processes
default  proc/stuck   integer           Stuck processes
default  net/in_kbs   integer   Kb/s    Network data in
default  net/in_pps   integer   pps     Network packets in
default  net/out_kbs  integer   Kb/s    Network data out
default  net/out_pps  integer   pps     Network packets out
default  io/rdops     integer   iops    Disk I/O (read)
default  io/wrops     integer   iops    Disk I/O (write)
default  mem/total    integer   KB      Total RAM
default  mem/free     integer   KB      Free RAM
default  hostname     string            Hostname
tenant   power/level  frac      %       Battery Level
tenant   power/src    string            Power Source
--------------------------------------------------------------------------------
```

Note that the type indicated with `--type` is a hint about how watchers should
interpret the value. The agent may end up encoding a `frac` value as an
`integer` if there’s no decimal point, this will not stop a watcher set to type
`frac` from correctly noticing it going over or under the limit.

With the extra information provided, your meter should now also show up in the
`host-show` non-JSON output:

```Apex
$ opticon host-show --host 0d19d114-55c8-4077-9cab-348579c70612
---( HOST )---------------------------------------------------------------------
UUID............: 0d19d114-55c8-4077-9cab-348579c70612
Hostname........: giskard.local
...
---( OTHER )--------------------------------------------------------------------
Battery Level...: 96.00 %
Power Source....: AC
--------------------------------------------------------------------------------
```

Now that the meter exists in the database, it’s also possible to set up watchers
for it. Let’s set up some sensible levels:

```Apex
$ opticon watcher-set --meter power/level --level warning --match lt --value 30
$ opticon watcher-set --meter power/level --level alert --match lt --value 15
$ opticon watcher-list | grep power/level
tenant   power/level  warning   lt                     30.00                1.0
tenant   power/level  alert     lt                     15.00                1.0
```

Table data
----------

If you want to send table-like metering data, like the process list, a little
more work needs to be done. Let’s walk in a fictional universe, where there is
no probe for the currently logged in users (there is). First we’ll write a
wrapper around the output of the “who” command, which looks like this on Darwin:

```Apex
$ who
pi       console  Oct  4 22:55 
pi       ttys000  Oct  5 10:27 
pi       ttys001  Oct  5 10:43 
pi       ttys002  Oct  5 00:18 
pi       ttys003  Oct  5 10:43  (172.16.1.10)
```

And the pox-ridden contraption of a bash script to convert it into JSON:

```Apex
$ cat /usr/local/scripts/who.sh
#!/bin/sh
echo '{"who":['
who | while read name tty month day time remote; do
  if [ ! -z "$remote" ]; then
    remote=$(echo "$remote" | cut -f2 -d'(' | cut -f1 -d')')
    printf '  {"user":"%s","tty":"%s","remote":"%s"}\n' "$name" "$tty" "$remote"
  fi
done
echo ']}'
$ /usr/local/scripts/who.sh
{"who":[
  {"user":"pi","tty":"ttys003","remote":"172.16.1.10"}
]}
```

We’ll bind it to a probe in `opticon-agent.conf` like before:

```
    who {
        type: exec
        call: /usr/local/scripts/who.sh
        interval: 60
    }
```

Finally, meters need to be set up. We’ll set one for each field in the table,
but also one for the table itself:

```Apex
$ opticon meter-create --meter who --type table --description "Remote Users"
$ opticon meter-create --meter who/user --type string --description "User"
$ opticon meter-create --meter who/tty --type string --description "TTY"
$ opticon meter-create --meter who/remote --type string --description "Remote IP"
```

With everything configured and the metering data coming in, the results should
be visible from `opticon host-show`:

```Apex
$ opticon host-show --host 0d19d114-55c8-4077-9cab-348579c70612
---( HOST )---------------------------------------------------------------------
UUID..............: 0d19d114-55c8-4077-9cab-348579c70612
Hostname..........: giskard.local
Address...........: ::ffff:92.108.228.195
...
---( STORAGE )------------------------------------------------------------------
DEVICE                 SIZE FS         USED MOUNTPOINT 
/dev/disk2        147.08 GB hfs     94.00 % / 
/dev/disk0s2      465.44 GB hfs     91.00 % /Volumes/Giskard Data 
---( REMOTE USERS )-------------------------------------------------------------
USER      TTY           REMOTE IP         
pi        ttys003       172.16.1.10       
--------------------------------------------------------------------------------
```
