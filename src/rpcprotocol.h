// copyright (c) 2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_rpcprotocol_h
#define moorecoin_rpcprotocol_h

#include <list>
#include <map>
#include <stdint.h>
#include <string>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include "univalue/univalue.h"

//! http status codes
enum httpstatuscode
{
    http_ok                    = 200,
    http_bad_request           = 400,
    http_unauthorized          = 401,
    http_forbidden             = 403,
    http_not_found             = 404,
    http_internal_server_error = 500,
    http_service_unavailable   = 503,
};

//! moorecoin rpc error codes
enum rpcerrorcode
{
    //! standard json-rpc 2.0 errors
    rpc_invalid_request  = -32600,
    rpc_method_not_found = -32601,
    rpc_invalid_params   = -32602,
    rpc_internal_error   = -32603,
    rpc_parse_error      = -32700,

    //! general application defined errors
    rpc_misc_error                  = -1,  //! std::exception thrown in command handling
    rpc_forbidden_by_safe_mode      = -2,  //! server is in safe mode, and command is not allowed in safe mode
    rpc_type_error                  = -3,  //! unexpected type was passed as parameter
    rpc_invalid_address_or_key      = -5,  //! invalid address or key
    rpc_out_of_memory               = -7,  //! ran out of memory during operation
    rpc_invalid_parameter           = -8,  //! invalid, missing or duplicate parameter
    rpc_database_error              = -20, //! database error
    rpc_deserialization_error       = -22, //! error parsing or validating structure in raw format
    rpc_verify_error                = -25, //! general error during transaction or block submission
    rpc_verify_rejected             = -26, //! transaction or block was rejected by network rules
    rpc_verify_already_in_chain     = -27, //! transaction already in chain
    rpc_in_warmup                   = -28, //! client still warming up

    //! aliases for backward compatibility
    rpc_transaction_error           = rpc_verify_error,
    rpc_transaction_rejected        = rpc_verify_rejected,
    rpc_transaction_already_in_chain= rpc_verify_already_in_chain,

    //! p2p client errors
    rpc_client_not_connected        = -9,  //! moorecoin is not connected
    rpc_client_in_initial_download  = -10, //! still downloading initial blocks
    rpc_client_node_already_added   = -23, //! node is already added
    rpc_client_node_not_added       = -24, //! node has not been added before
    rpc_client_node_not_connected   = -29, //! node to disconnect not found in connected nodes
    rpc_client_invalid_ip_or_subnet = -30, //! invalid ip/subnet

    //! wallet errors
    rpc_wallet_error                = -4,  //! unspecified problem with wallet (key not found etc.)
    rpc_wallet_insufficient_funds   = -6,  //! not enough funds in wallet or account
    rpc_wallet_invalid_account_name = -11, //! invalid account name
    rpc_wallet_keypool_ran_out      = -12, //! keypool ran out, call keypoolrefill first
    rpc_wallet_unlock_needed        = -13, //! enter the wallet passphrase with walletpassphrase first
    rpc_wallet_passphrase_incorrect = -14, //! the wallet passphrase entered was incorrect
    rpc_wallet_wrong_enc_state      = -15, //! command given in wrong wallet encryption state (encrypting an encrypted wallet etc.)
    rpc_wallet_encryption_failed    = -16, //! failed to encrypt the wallet
    rpc_wallet_already_unlocked     = -17, //! wallet is already unlocked
};

/**
 * iostream device that speaks ssl but can also speak non-ssl
 */
template <typename protocol>
class ssliostreamdevice : public boost::iostreams::device<boost::iostreams::bidirectional> {
public:
    ssliostreamdevice(boost::asio::ssl::stream<typename protocol::socket> &streamin, bool fusesslin) : stream(streamin)
    {
        fusessl = fusesslin;
        fneedhandshake = fusesslin;
    }

    void handshake(boost::asio::ssl::stream_base::handshake_type role)
    {
        if (!fneedhandshake) return;
        fneedhandshake = false;
        stream.handshake(role);
    }
    std::streamsize read(char* s, std::streamsize n)
    {
        handshake(boost::asio::ssl::stream_base::server); // https servers read first
        if (fusessl) return stream.read_some(boost::asio::buffer(s, n));
        return stream.next_layer().read_some(boost::asio::buffer(s, n));
    }
    std::streamsize write(const char* s, std::streamsize n)
    {
        handshake(boost::asio::ssl::stream_base::client); // https clients write first
        if (fusessl) return boost::asio::write(stream, boost::asio::buffer(s, n));
        return boost::asio::write(stream.next_layer(), boost::asio::buffer(s, n));
    }
    bool connect(const std::string& server, const std::string& port)
    {
        using namespace boost::asio::ip;
        tcp::resolver resolver(stream.get_io_service());
        tcp::resolver::iterator endpoint_iterator;
#if boost_version >= 104300
        try {
#endif
            // the default query (flags address_configured) tries ipv6 if
            // non-localhost ipv6 configured, and ipv4 if non-localhost ipv4
            // configured.
            tcp::resolver::query query(server.c_str(), port.c_str());
            endpoint_iterator = resolver.resolve(query);
#if boost_version >= 104300
        } catch (const boost::system::system_error&) {
            // if we at first don't succeed, try blanket lookup (ipv4+ipv6 independent of configured interfaces)
            tcp::resolver::query query(server.c_str(), port.c_str(), resolver_query_base::flags());
            endpoint_iterator = resolver.resolve(query);
        }
#endif
        boost::system::error_code error = boost::asio::error::host_not_found;
        tcp::resolver::iterator end;
        while (error && endpoint_iterator != end)
        {
            stream.lowest_layer().close();
            stream.lowest_layer().connect(*endpoint_iterator++, error);
        }
        if (error)
            return false;
        return true;
    }

private:
    bool fneedhandshake;
    bool fusessl;
    boost::asio::ssl::stream<typename protocol::socket>& stream;
};

std::string httppost(const std::string& strmsg, const std::map<std::string,std::string>& maprequestheaders);
std::string httperror(int nstatus, bool keepalive,
                      bool headeronly = false);
std::string httpreplyheader(int nstatus, bool keepalive, size_t contentlength,
                      const char *contenttype = "application/json");
std::string httpreply(int nstatus, const std::string& strmsg, bool keepalive,
                      bool headeronly = false,
                      const char *contenttype = "application/json");
bool readhttprequestline(std::basic_istream<char>& stream, int &proto,
                         std::string& http_method, std::string& http_uri);
int readhttpstatus(std::basic_istream<char>& stream, int &proto);
int readhttpheaders(std::basic_istream<char>& stream, std::map<std::string, std::string>& mapheadersret);
int readhttpmessage(std::basic_istream<char>& stream, std::map<std::string, std::string>& mapheadersret,
                    std::string& strmessageret, int nproto, size_t max_size);
std::string jsonrpcrequest(const std::string& strmethod, const univalue& params, const univalue& id);
univalue jsonrpcreplyobj(const univalue& result, const univalue& error, const univalue& id);
std::string jsonrpcreply(const univalue& result, const univalue& error, const univalue& id);
univalue jsonrpcerror(int code, const std::string& message);

#endif // moorecoin_rpcprotocol_h
