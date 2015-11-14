// copyright (c) 2011-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

//
// unit tests for block-chain checkpoints
//

#include "checkpoints.h"

#include "uint256.h"
#include "test/test_moorecoin.h"
#include "chainparams.h"

#include <boost/test/unit_test.hpp>

using namespace std;

boost_fixture_test_suite(checkpoints_tests, basictestingsetup)

boost_auto_test_case(sanity)
{
    const checkpoints::ccheckpointdata& checkpoints = params(cbasechainparams::main).checkpoints();
    boost_check(checkpoints::gettotalblocksestimate(checkpoints) >= 134444);
}

boost_auto_test_suite_end()
