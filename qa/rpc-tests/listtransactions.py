#!/usr/bin/env python2
# copyright (c) 2014 the moorecoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.

# exercise the listtransactions api

from test_framework.test_framework import moorecointestframework
from test_framework.util import *


def check_array_result(object_array, to_match, expected):
    """
    pass in array of json objects, a dictionary with key/value pairs
    to match against, and another dictionary with expected key/value
    pairs.
    """
    num_matched = 0
    for item in object_array:
        all_match = true
        for key,value in to_match.items():
            if item[key] != value:
                all_match = false
        if not all_match:
            continue
        for key,value in expected.items():
            if item[key] != value:
                raise assertionerror("%s : expected %s=%s"%(str(item), str(key), str(value)))
            num_matched = num_matched+1
    if num_matched == 0:
        raise assertionerror("no objects matched %s"%(str(to_match)))

class listtransactionstest(moorecointestframework):

    def run_test(self):
        # simple send, 0 to 1:
        txid = self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 0.1)
        self.sync_all()
        check_array_result(self.nodes[0].listtransactions(),
                           {"txid":txid},
                           {"category":"send","account":"","amount":decimal("-0.1"),"confirmations":0})
        check_array_result(self.nodes[1].listtransactions(),
                           {"txid":txid},
                           {"category":"receive","account":"","amount":decimal("0.1"),"confirmations":0})
        # mine a block, confirmations should change:
        self.nodes[0].generate(1)
        self.sync_all()
        check_array_result(self.nodes[0].listtransactions(),
                           {"txid":txid},
                           {"category":"send","account":"","amount":decimal("-0.1"),"confirmations":1})
        check_array_result(self.nodes[1].listtransactions(),
                           {"txid":txid},
                           {"category":"receive","account":"","amount":decimal("0.1"),"confirmations":1})

        # send-to-self:
        txid = self.nodes[0].sendtoaddress(self.nodes[0].getnewaddress(), 0.2)
        check_array_result(self.nodes[0].listtransactions(),
                           {"txid":txid, "category":"send"},
                           {"amount":decimal("-0.2")})
        check_array_result(self.nodes[0].listtransactions(),
                           {"txid":txid, "category":"receive"},
                           {"amount":decimal("0.2")})

        # sendmany from node1: twice to self, twice to node2:
        send_to = { self.nodes[0].getnewaddress() : 0.11,
                    self.nodes[1].getnewaddress() : 0.22,
                    self.nodes[0].getaccountaddress("from1") : 0.33,
                    self.nodes[1].getaccountaddress("toself") : 0.44 }
        txid = self.nodes[1].sendmany("", send_to)
        self.sync_all()
        check_array_result(self.nodes[1].listtransactions(),
                           {"category":"send","amount":decimal("-0.11")},
                           {"txid":txid} )
        check_array_result(self.nodes[0].listtransactions(),
                           {"category":"receive","amount":decimal("0.11")},
                           {"txid":txid} )
        check_array_result(self.nodes[1].listtransactions(),
                           {"category":"send","amount":decimal("-0.22")},
                           {"txid":txid} )
        check_array_result(self.nodes[1].listtransactions(),
                           {"category":"receive","amount":decimal("0.22")},
                           {"txid":txid} )
        check_array_result(self.nodes[1].listtransactions(),
                           {"category":"send","amount":decimal("-0.33")},
                           {"txid":txid} )
        check_array_result(self.nodes[0].listtransactions(),
                           {"category":"receive","amount":decimal("0.33")},
                           {"txid":txid, "account" : "from1"} )
        check_array_result(self.nodes[1].listtransactions(),
                           {"category":"send","amount":decimal("-0.44")},
                           {"txid":txid, "account" : ""} )
        check_array_result(self.nodes[1].listtransactions(),
                           {"category":"receive","amount":decimal("0.44")},
                           {"txid":txid, "account" : "toself"} )

if __name__ == '__main__':
    listtransactionstest().main()

