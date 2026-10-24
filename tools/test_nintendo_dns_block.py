"""Check the production block rules against the original 90DNS test domains.

Uses Atmosphere's documented '*' matching and last-match-wins semantics, not
real DNS or Nintendo traffic. Hardware DNS interception is a separate test.
"""
import fnmatch,subprocess,sys,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
domains='''nintendo.com nintendo.net nintendo.jp nintendo.co.jp nintendo.co.uk
nintendo-europe.com nintendowifi.net nintendo.es nintendo.co.kr nintendo.tw
nintendo.com.hk nintendo.com.au nintendo.co.nz nintendo.at nintendo.be
nintendods.cz nintendo.dk nintendo.de nintendo.fi nintendo.fr nintendo.gr
nintendo.hu nintendo.it nintendo.nl nintendo.no nintendo.pt nintendo.ru
nintendo.co.za nintendo.se nintendo.ch nintendo.pl nintendoswitch.com
nintendoswitch.com.cn nintendoswitch.cn sun.hac.lp1.d4c.nintendo.net'''.split()
code=r'''
#include <stdio.h>
#include "prelude_dns_block.h"
int main(void) {
 char *hosts=prelude_nintendo_block_hosts("# retained user entry\n192.0.2.1 custom.example\n192.0.2.2 *.nintendo.*");
 if(!hosts) return 1;
 fputs(hosts,stdout);free(hosts);return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='prelude-dns-') as directory:
 folder=Path(directory);source=folder/'test.c';binary=folder/('test.exe' if sys.platform=='win32' else 'test')
 source.write_text(code,encoding='utf-8')
 subprocess.run([sys.argv[1] if len(sys.argv)>1 else 'cc','-std=c11','-Wall','-Wextra','-Werror','-I',str(root/'source'),str(source),'-o',str(binary)],check=True)
 hosts=subprocess.check_output([str(binary)],text=True)
def resolve(domain):
 result=None
 for line in hosts.splitlines():
  fields=line.split('#',1)[0].split()
  if len(fields)<2:continue
  for pattern in fields[1:]:
   if fnmatch.fnmatchcase(domain,pattern):result=fields[0]
 return result
for domain in domains+['accounts.nintendo.com','api.accounts.nintendo.com','dauth-lp1.ndas.srv.nintendo.net','aauth.hac.lp1.ndas.srv.nintendo.net','receive-lp1.dg.srv.nintendo.net']:
 assert resolve(domain)=='127.0.0.1',domain
assert resolve('custom.example')=='192.0.2.1'
for domain in ('nextendo.network','github.com','api.github.com','example.com'):
 assert resolve(domain) is None,domain
print(f'PASS: all {len(domains)} original 90DNS domains and account/telemetry endpoints blocked; custom entries retained; earlier Nintendo redirects overridden; unrelated DNS unaffected')
