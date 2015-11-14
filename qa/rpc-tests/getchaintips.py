#!/usr/bin/env python2
# copyright (c) 2014 the moorecoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.

# exercise the getchaintips api.  we introduce a network split, work
# on chains of different lengths, and join the network together again.
# this gives us two tips, verify that it works.

from test_framework.test_framework import moorecointestframework
from test_framework.util import assert_equal

class getchaintipstest (moorecointestframework):

    def run_test (self):
        moorecointestframework.run_test (self)

        tips = self.nodes[0].getchaintips ()
        assert_equal (len (tips), 1)
        assert_equal (tips[0]['branchlen'], 0)
        assert_equal (tips[0]['height'], 200)
        assert_equal (tips[0]['status'], 'active')

        # split the network and build two chains of different lengths.
        self.split_network ()
        self.nodes[0].generate(10);
        self.nodes[2].generate(20);
        self.sync_all ()

        tips = self.nodes[1].getchaintips ()
        assert_equal (len (tips), 1)
        shorttip = tips[0]
        assert_equal (shorttip['branchlen'], 0)
        assert_equal (shorttip['height'], 210)
        assert_equal (tips[0]['status'], 'active')

        tips = self.nodes[3].getchaintips ()
        assert_equal (len (tips), 1)
        longtip = tips[0]
        assert_equal (longtip['branchlen'], 0)
        assert_equal (longtip['height'], 220)
        assert_equal (tips[0]['status'], 'active')

        # join the network halves and check that we now have two tips
        # (at least at the nodes that previously had the short chain).
        self.join_network ()

        tips = self.nodes[0].getchaintips ()
        assert_equal (len (tips), 2)
        assert_equal (tips[0], longtip)

        assert_equal (tips[1]['branchlen'], 10)
        assert_equal (tips[1]['status'], 'valid-fork')
        tips[1]['branchlen'] = 0
        tips[1]['status'] = 'active'
        assert_equal (tips[1], shorttip)

if __name__ == '__main__':
    getchaintipstest ().main ()
