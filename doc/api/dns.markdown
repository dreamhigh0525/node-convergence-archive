# DNS

    Stability: 3 - Stable

Use `require('dns')` to access this module. All methods in the dns module use
C-Ares except for `dns.lookup` which uses `getaddrinfo(3)` in a thread pool.
C-Ares is much faster than `getaddrinfo(3)` but, due to the way it is
configured by node, it doesn't use the same set of system configuration files.
For instance, _C- Ares will not use the configuration from `/etc/hosts`_. As a
result, __only `dns.lookup` should be expected to behave like other programs
running on the same system regarding name resolution.__ When a user does
`net.connect(80, 'google.com')` or `http.get({ host: 'google.com' })` the
`dns.lookup` method is used. Users who need to do a large number of lookups
quickly should use the methods that go through C-Ares.

Here is an example which resolves `'www.google.com'` then reverse
resolves the IP addresses which are returned.

    var dns = require('dns');

    dns.resolve4('www.google.com', function (err, addresses) {
      if (err) throw err;

      console.log('addresses: ' + JSON.stringify(addresses));

      addresses.forEach(function (a) {
        dns.reverse(a, function (err, hostnames) {
          if (err) {
            throw err;
          }

          console.log('reverse for ' + a + ': ' + JSON.stringify(hostnames));
        });
      });
    });

## dns.lookup(hostname[, options], callback)

Resolves a hostname (e.g. `'google.com'`) into the first found A (IPv4) or
AAAA (IPv6) record. `options` can be an object or integer. If `options` is
not provided, then IP v4 and v6 addresses are both valid. If `options` is
an integer, then it must be `4` or `6`.

Alternatively, `options` can be an object containing two properties,
`family` and `hints`. Both properties are optional. If `family` is provided,
it must be the integer `4` or `6`. If `family` is not provided then IP v4
and v6 addresses are accepted. The `hints` field, if present, should be one
or more of the supported `getaddrinfo` flags. If `hints` is not provided,
then no flags are passed to `getaddrinfo`. Multiple flags can be passed
through `hints` by logically `OR`ing their values. An example usage of
`options` is shown below.

```
{
  family: 4,
  hints: dns.ADDRCONFIG | dns.V4MAPPED
}
```

See [supported `getaddrinfo` flags](#dns_supported_getaddrinfo_flags) below for
more information on supported flags.

The callback has arguments `(err, address, family)`.  The `address` argument
is a string representation of a IP v4 or v6 address. The `family` argument
is either the integer 4 or 6 and denotes the family of `address` (not
necessarily the value initially passed to `lookup`).

On error, `err` is an `Error` object, where `err.code` is the error code.
Keep in mind that `err.code` will be set to `'ENOENT'` not only when
the hostname does not exist but also when the lookup fails in other ways
such as no available file descriptors.


# dns.lookupService(address, port, callback)

Resolves the given address and port into a hostname and service using
`getnameinfo`.

The callback has arguments `(err, hostname, service)`. The `hostname` and
`service` arguments are strings (e.g. `'localhost'` and `'http'` respectively).

On error, `err` is an `Error` object, where `err.code` is the error code.


## dns.resolve(hostname[, rrtype], callback)

Resolves a hostname (e.g. `'google.com'`) into an array of the record types
specified by rrtype.

Valid rrtypes are:

 * `'A'` (IPV4 addresses, default)
 * `'AAAA'` (IPV6 addresses)
 * `'MX'` (mail exchange records)
 * `'TXT'` (text records)
 * `'SRV'` (SRV records)
 * `'PTR'` (used for reverse IP lookups)
 * `'NS'` (name server records)
 * `'CNAME'` (canonical name records)
 * `'SOA'` (start of authority record)

The callback has arguments `(err, addresses)`.  The type of each item
in `addresses` is determined by the record type, and described in the
documentation for the corresponding lookup methods below.

On error, `err` is an `Error` object, where `err.code` is
one of the error codes listed below.


## dns.resolve4(hostname, callback)

The same as `dns.resolve()`, but only for IPv4 queries (`A` records).
`addresses` is an array of IPv4 addresses (e.g.
`['74.125.79.104', '74.125.79.105', '74.125.79.106']`).

## dns.resolve6(hostname, callback)

The same as `dns.resolve4()` except for IPv6 queries (an `AAAA` query).


## dns.resolveMx(hostname, callback)

The same as `dns.resolve()`, but only for mail exchange queries (`MX` records).

`addresses` is an array of MX records, each with a priority and an exchange
attribute (e.g. `[{'priority': 10, 'exchange': 'mx.example.com'},...]`).

## dns.resolveTxt(hostname, callback)

The same as `dns.resolve()`, but only for text queries (`TXT` records).
`addresses` is an 2-d array of the text records available for `hostname` (e.g.,
`[ ['v=spf1 ip4:0.0.0.0 ', '~all' ] ]`). Each sub-array contains TXT chunks of
one record. Depending on the use case, the could be either joined together or
treated separately.

## dns.resolveSrv(hostname, callback)

The same as `dns.resolve()`, but only for service records (`SRV` records).
`addresses` is an array of the SRV records available for `hostname`. Properties
of SRV records are priority, weight, port, and name (e.g.,
`[{'priority': 10, 'weight': 5, 'port': 21223, 'name': 'service.example.com'}, ...]`).

## dns.resolveSoa(hostname, callback)

The same as `dns.resolve()`, but only for start of authority record queries
(`SOA` record).

`addresses` is an object with the following structure:

```
{
  nsname: 'ns.example.com',
  hostmaster: 'root.example.com',
  serial: 2013101809,
  refresh: 10000,
  retry: 2400,
  expire: 604800,
  minttl: 3600
}
```

## dns.resolveNs(hostname, callback)

The same as `dns.resolve()`, but only for name server records (`NS` records).
`addresses` is an array of the name server records available for `hostname`
(e.g., `['ns1.example.com', 'ns2.example.com']`).

## dns.resolveCname(hostname, callback)

The same as `dns.resolve()`, but only for canonical name records (`CNAME`
records). `addresses` is an array of the canonical name records available for
`hostname` (e.g., `['bar.example.com']`).

## dns.reverse(ip, callback)

Reverse resolves an ip address to an array of hostnames.

The callback has arguments `(err, hostnames)`.

On error, `err` is an `Error` object, where `err.code` is
one of the error codes listed below.

## dns.getServers()

Returns an array of IP addresses as strings that are currently being used for
resolution

## dns.setServers(servers)

Given an array of IP addresses as strings, set them as the servers to use for
resolving

If you specify a port with the address it will be stripped, as the underlying
library doesn't support that.

This will throw if you pass invalid input.

## Error codes

Each DNS query can return one of the following error codes:

- `dns.NODATA`: DNS server returned answer with no data.
- `dns.FORMERR`: DNS server claims query was misformatted.
- `dns.SERVFAIL`: DNS server returned general failure.
- `dns.NOTFOUND`: Domain name not found.
- `dns.NOTIMP`: DNS server does not implement requested operation.
- `dns.REFUSED`: DNS server refused query.
- `dns.BADQUERY`: Misformatted DNS query.
- `dns.BADNAME`: Misformatted hostname.
- `dns.BADFAMILY`: Unsupported address family.
- `dns.BADRESP`: Misformatted DNS reply.
- `dns.CONNREFUSED`: Could not contact DNS servers.
- `dns.TIMEOUT`: Timeout while contacting DNS servers.
- `dns.EOF`: End of file.
- `dns.FILE`: Error reading file.
- `dns.NOMEM`: Out of memory.
- `dns.DESTRUCTION`: Channel is being destroyed.
- `dns.BADSTR`: Misformatted string.
- `dns.BADFLAGS`: Illegal flags specified.
- `dns.NONAME`: Given hostname is not numeric.
- `dns.BADHINTS`: Illegal hints flags specified.
- `dns.NOTINITIALIZED`: c-ares library initialization not yet performed.
- `dns.LOADIPHLPAPI`: Error loading iphlpapi.dll.
- `dns.ADDRGETNETWORKPARAMS`: Could not find GetNetworkParams function.
- `dns.CANCELLED`: DNS query cancelled.

## Supported getaddrinfo flags

The following flags can be passed as hints to `dns.lookup`.

- `dns.ADDRCONFIG`: Returned address types are determined by the types
of addresses supported by the current system. For example, IPv4 addresses
are only returned if the current system has at least one IPv4 address
configured. Loopback addresses are not considered.
- `dns.V4MAPPED`: If the IPv6 family was specified, but no IPv6 addresses
were found, then return IPv4 mapped IPv6 addresses.
