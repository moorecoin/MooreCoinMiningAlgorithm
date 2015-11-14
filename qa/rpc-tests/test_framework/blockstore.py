# blockstore: a helper class that keeps a map of blocks and implements
#             helper functions for responding to getheaders and getdata,
#             and for constructing a getheaders message
#

from mininode import *
import dbm

class blockstore(object):
    def __init__(self, datadir):
        self.blockdb = dbm.open(datadir + "/blocks", 'c')
        self.currentblock = 0l
    
    def close(self):
        self.blockdb.close()

    def get(self, blockhash):
        serialized_block = none
        try:
            serialized_block = self.blockdb[repr(blockhash)]
        except keyerror:
            return none
        f = cstringio.stringio(serialized_block)
        ret = cblock()
        ret.deserialize(f)
        ret.calc_sha256()
        return ret

    # note: this pulls full blocks out of the database just to retrieve
    # the headers -- perhaps we could keep a separate data structure
    # to avoid this overhead.
    def headers_for(self, locator, hash_stop, current_tip=none):
        if current_tip is none:
            current_tip = self.currentblock
        current_block = self.get(current_tip)
        if current_block is none:
            return none

        response = msg_headers()
        headerslist = [ cblockheader(current_block) ]
        maxheaders = 2000
        while (headerslist[0].sha256 not in locator.vhave):
            prevblockhash = headerslist[0].hashprevblock
            prevblock = self.get(prevblockhash)
            if prevblock is not none:
                headerslist.insert(0, cblockheader(prevblock))
            else:
                break
        headerslist = headerslist[:maxheaders] # truncate if we have too many
        hashlist = [x.sha256 for x in headerslist]
        index = len(headerslist)
        if (hash_stop in hashlist):
            index = hashlist.index(hash_stop)+1
        response.headers = headerslist[:index]
        return response

    def add_block(self, block):
        block.calc_sha256()
        try:
            self.blockdb[repr(block.sha256)] = bytes(block.serialize())
        except typeerror as e:
            print "unexpected error: ", sys.exc_info()[0], e.args
        self.currentblock = block.sha256

    def get_blocks(self, inv):
        responses = []
        for i in inv:
            if (i.type == 2): # msg_block
                block = self.get(i.hash)
                if block is not none:
                    responses.append(msg_block(block))
        return responses

    def get_locator(self, current_tip=none):
        if current_tip is none:
            current_tip = self.currentblock
        r = []
        counter = 0
        step = 1
        lastblock = self.get(current_tip)
        while lastblock is not none:
            r.append(lastblock.hashprevblock)
            for i in range(step):
                lastblock = self.get(lastblock.hashprevblock)
                if lastblock is none:
                    break
            counter += 1
            if counter > 10:
                step *= 2
        locator = cblocklocator()
        locator.vhave = r
        return locator

class txstore(object):
    def __init__(self, datadir):
        self.txdb = dbm.open(datadir + "/transactions", 'c')

    def close(self):
        self.txdb.close()

    def get(self, txhash):
        serialized_tx = none
        try:
            serialized_tx = self.txdb[repr(txhash)]
        except keyerror:
            return none
        f = cstringio.stringio(serialized_tx)
        ret = ctransaction()
        ret.deserialize(f)
        ret.calc_sha256()
        return ret

    def add_transaction(self, tx):
        tx.calc_sha256()
        try:
            self.txdb[repr(tx.sha256)] = bytes(tx.serialize())
        except typeerror as e:
            print "unexpected error: ", sys.exc_info()[0], e.args

    def get_transactions(self, inv):
        responses = []
        for i in inv:
            if (i.type == 1): # msg_tx
                tx = self.get(i.hash)
                if tx is not none:
                    responses.append(msg_tx(tx))
        return responses
