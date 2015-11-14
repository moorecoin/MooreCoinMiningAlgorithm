#!/usr/bin/env python2
# copyright (c) 2014 the moorecoin core developers
# distributed under the mit software license, see the accompanying
# file copying or http://www.opensource.org/licenses/mit-license.php.

# exercise the listreceivedbyaddress api

from test_framework.test_framework import moorecointestframework
from test_framework.util import *


def get_sub_array_from_array(object_array, to_match):
    '''
        finds and returns a sub array from an array of arrays.
        to_match should be a unique idetifier of a sub array
    '''
    num_matched = 0
    for item in object_array:
        all_match = true
        for key,value in to_match.items():
            if item[key] != value:
                all_match = false
        if not all_match:
            continue
        return item
    return []

def check_array_result(object_array, to_match, expected, should_not_find = false):
    """
        pass in array of json objects, a dictionary with key/value pairs
        to match against, and another dictionary with expected key/value
        pairs.
        if the should_not_find flag is true, to_match should not be found in object_array
        """
    if should_not_find == true:
        expected = { }
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
    if num_matched == 0 and should_not_find != true:
        raise assertionerror("no objects matched %s"%(str(to_match)))
    if num_matched > 0 and should_not_find == true:
        raise assertionerror("objects was matched %s"%(str(to_match)))

class receivedbytest(moorecointestframework):

    def run_test(self):
        '''
        listreceivedbyaddress test
        '''
        # send from node 0 to 1
        addr = self.nodes[1].getnewaddress()
        txid = self.nodes[0].sendtoaddress(addr, 0.1)
        self.sync_all()

        #check not listed in listreceivedbyaddress because has 0 confirmations
        check_array_result(self.nodes[1].listreceivedbyaddress(),
                           {"address":addr},
                           { },
                           true)
        #bury tx under 10 block so it will be returned by listreceivedbyaddress
        self.nodes[1].generate(10)
        self.sync_all()
        check_array_result(self.nodes[1].listreceivedbyaddress(),
                           {"address":addr},
                           {"address":addr, "account":"", "amount":decimal("0.1"), "confirmations":10, "txids":[txid,]})
        #with min confidence < 10
        check_array_result(self.nodes[1].listreceivedbyaddress(5),
                           {"address":addr},
                           {"address":addr, "account":"", "amount":decimal("0.1"), "confirmations":10, "txids":[txid,]})
        #with min confidence > 10, should not find tx
        check_array_result(self.nodes[1].listreceivedbyaddress(11),{"address":addr},{ },true)

        #empty tx
        addr = self.nodes[1].getnewaddress()
        check_array_result(self.nodes[1].listreceivedbyaddress(0,true),
                           {"address":addr},
                           {"address":addr, "account":"", "amount":0, "confirmations":0, "txids":[]})

        '''
            getreceivedbyaddress test
        '''
        # send from node 0 to 1
        addr = self.nodes[1].getnewaddress()
        txid = self.nodes[0].sendtoaddress(addr, 0.1)
        self.sync_all()

        #check balance is 0 because of 0 confirmations
        balance = self.nodes[1].getreceivedbyaddress(addr)
        if balance != decimal("0.0"):
            raise assertionerror("wrong balance returned by getreceivedbyaddress, %0.2f"%(balance))

        #check balance is 0.1
        balance = self.nodes[1].getreceivedbyaddress(addr,0)
        if balance != decimal("0.1"):
            raise assertionerror("wrong balance returned by getreceivedbyaddress, %0.2f"%(balance))

        #bury tx under 10 block so it will be returned by the default getreceivedbyaddress
        self.nodes[1].generate(10)
        self.sync_all()
        balance = self.nodes[1].getreceivedbyaddress(addr)
        if balance != decimal("0.1"):
            raise assertionerror("wrong balance returned by getreceivedbyaddress, %0.2f"%(balance))

        '''
            listreceivedbyaccount + getreceivedbyaccount test
        '''
        #set pre-state
        addrarr = self.nodes[1].getnewaddress()
        account = self.nodes[1].getaccount(addrarr)
        received_by_account_json = get_sub_array_from_array(self.nodes[1].listreceivedbyaccount(),{"account":account})
        if len(received_by_account_json) == 0:
            raise assertionerror("no accounts found in node")
        balance_by_account = rec_by_accountarr = self.nodes[1].getreceivedbyaccount(account)

        txid = self.nodes[0].sendtoaddress(addr, 0.1)
        self.sync_all()

        # listreceivedbyaccount should return received_by_account_json because of 0 confirmations
        check_array_result(self.nodes[1].listreceivedbyaccount(),
                           {"account":account},
                           received_by_account_json)

        # getreceivedbyaddress should return same balance because of 0 confirmations
        balance = self.nodes[1].getreceivedbyaccount(account)
        if balance != balance_by_account:
            raise assertionerror("wrong balance returned by getreceivedbyaccount, %0.2f"%(balance))

        self.nodes[1].generate(10)
        self.sync_all()
        # listreceivedbyaccount should return updated account balance
        check_array_result(self.nodes[1].listreceivedbyaccount(),
                           {"account":account},
                           {"account":received_by_account_json["account"], "amount":(received_by_account_json["amount"] + decimal("0.1"))})

        # getreceivedbyaddress should return updates balance
        balance = self.nodes[1].getreceivedbyaccount(account)
        if balance != balance_by_account + decimal("0.1"):
            raise assertionerror("wrong balance returned by getreceivedbyaccount, %0.2f"%(balance))

        #create a new account named "mynewaccount" that has a 0 balance
        self.nodes[1].getaccountaddress("mynewaccount")
        received_by_account_json = get_sub_array_from_array(self.nodes[1].listreceivedbyaccount(0,true),{"account":"mynewaccount"})
        if len(received_by_account_json) == 0:
            raise assertionerror("no accounts found in node")

        # test includeempty of listreceivedbyaccount
        if received_by_account_json["amount"] != decimal("0.0"):
            raise assertionerror("wrong balance returned by listreceivedbyaccount, %0.2f"%(received_by_account_json["amount"]))

        # test getreceivedbyaccount for 0 amount accounts
        balance = self.nodes[1].getreceivedbyaccount("mynewaccount")
        if balance != decimal("0.0"):
            raise assertionerror("wrong balance returned by getreceivedbyaccount, %0.2f"%(balance))

if __name__ == '__main__':
    receivedbytest().main()
