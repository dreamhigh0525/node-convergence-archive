node.mixin(require("../common.js"));
var dns = require("/dns.js");

for (var i = 2; i < ARGV.length; i++) {
  var name = ARGV[i]
  puts("looking up " + name);
  var resolution = dns.resolve4(name);

  resolution.addCallback(function (addresses, ttl, cname) {
    puts("addresses: " + JSON.stringify(addresses));
    puts("ttl: " + JSON.stringify(ttl));
    puts("cname: " + JSON.stringify(cname));

    for (var i = 0; i < addresses.length; i++) {
      var a = addresses[i];
      var reversing = dns.reverse(a);
      reversing.addCallback( function (domains, ttl, cname) {
        puts("reverse for " + a + ": " + JSON.stringify(domains));
      });
      reversing.addErrback( function (code, msg) {
        puts("reverse for " + a + " failed: " + msg);
      });
    }
  });

  resolution.addErrback(function (code, msg) {
    puts("error: " + msg);
  });
}
