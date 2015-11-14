// copyright (c) 2011-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

//
// wraps dumb protocol buffer paymentrequest
// with some extra methods
//

#include "paymentrequestplus.h"

#include "util.h"

#include <stdexcept>

#include <openssl/x509_vfy.h>

#include <qdatetime>
#include <qdebug>
#include <qsslcertificate>

using namespace std;

class sslverifyerror : public std::runtime_error
{
public:
    sslverifyerror(std::string err) : std::runtime_error(err) { }
};

bool paymentrequestplus::parse(const qbytearray& data)
{
    bool parseok = paymentrequest.parsefromarray(data.data(), data.size());
    if (!parseok) {
        qwarning() << "paymentrequestplus::parse: error parsing payment request";
        return false;
    }
    if (paymentrequest.payment_details_version() > 1) {
        qwarning() << "paymentrequestplus::parse: received up-version payment details, version=" << paymentrequest.payment_details_version();
        return false;
    }

    parseok = details.parsefromstring(paymentrequest.serialized_payment_details());
    if (!parseok)
    {
        qwarning() << "paymentrequestplus::parse: error parsing payment details";
        paymentrequest.clear();
        return false;
    }
    return true;
}

bool paymentrequestplus::serializetostring(string* output) const
{
    return paymentrequest.serializetostring(output);
}

bool paymentrequestplus::isinitialized() const
{
    return paymentrequest.isinitialized();
}

bool paymentrequestplus::getmerchant(x509_store* certstore, qstring& merchant) const
{
    merchant.clear();

    if (!isinitialized())
        return false;

    // one day we'll support more pki types, but just
    // x509 for now:
    const evp_md* digestalgorithm = null;
    if (paymentrequest.pki_type() == "x509+sha256") {
        digestalgorithm = evp_sha256();
    }
    else if (paymentrequest.pki_type() == "x509+sha1") {
        digestalgorithm = evp_sha1();
    }
    else if (paymentrequest.pki_type() == "none") {
        qwarning() << "paymentrequestplus::getmerchant: payment request: pki_type == none";
        return false;
    }
    else {
        qwarning() << "paymentrequestplus::getmerchant: payment request: unknown pki_type " << qstring::fromstdstring(paymentrequest.pki_type());
        return false;
    }

    payments::x509certificates certchain;
    if (!certchain.parsefromstring(paymentrequest.pki_data())) {
        qwarning() << "paymentrequestplus::getmerchant: payment request: error parsing pki_data";
        return false;
    }

    std::vector<x509*> certs;
    const qdatetime currenttime = qdatetime::currentdatetime();
    for (int i = 0; i < certchain.certificate_size(); i++) {
        qbytearray certdata(certchain.certificate(i).data(), certchain.certificate(i).size());
        qsslcertificate qcert(certdata, qssl::der);
        if (currenttime < qcert.effectivedate() || currenttime > qcert.expirydate()) {
            qwarning() << "paymentrequestplus::getmerchant: payment request: certificate expired or not yet active: " << qcert;
            return false;
        }
#if qt_version >= 0x050000
        if (qcert.isblacklisted()) {
            qwarning() << "paymentrequestplus::getmerchant: payment request: certificate blacklisted: " << qcert;
            return false;
        }
#endif
        const unsigned char *data = (const unsigned char *)certchain.certificate(i).data();
        x509 *cert = d2i_x509(null, &data, certchain.certificate(i).size());
        if (cert)
            certs.push_back(cert);
    }
    if (certs.empty()) {
        qwarning() << "paymentrequestplus::getmerchant: payment request: empty certificate chain";
        return false;
    }

    // the first cert is the signing cert, the rest are untrusted certs that chain
    // to a valid root authority. openssl needs them separately.
    stack_of(x509) *chain = sk_x509_new_null();
    for (int i = certs.size() - 1; i > 0; i--) {
        sk_x509_push(chain, certs[i]);
    }
    x509 *signing_cert = certs[0];

    // now create a "store context", which is a single use object for checking,
    // load the signing cert into it and verify.
    x509_store_ctx *store_ctx = x509_store_ctx_new();
    if (!store_ctx) {
        qwarning() << "paymentrequestplus::getmerchant: payment request: error creating x509_store_ctx";
        return false;
    }

    char *website = null;
    bool fresult = true;
    try
    {
        if (!x509_store_ctx_init(store_ctx, certstore, signing_cert, chain))
        {
            int error = x509_store_ctx_get_error(store_ctx);
            throw sslverifyerror(x509_verify_cert_error_string(error));
        }

        // now do the verification!
        int result = x509_verify_cert(store_ctx);
        if (result != 1) {
            int error = x509_store_ctx_get_error(store_ctx);
            // for testing payment requests, we allow self signed root certs!
            // this option is just shown in the ui options, if -help-debug is enabled.
            if (!(error == x509_v_err_depth_zero_self_signed_cert && getboolarg("-allowselfsignedrootcertificates", false))) {
                throw sslverifyerror(x509_verify_cert_error_string(error));
            } else {
               qdebug() << "paymentrequestplus::getmerchant: allowing self signed root certificate, because -allowselfsignedrootcertificates is true.";
            }
        }
        x509_name *certname = x509_get_subject_name(signing_cert);

        // valid cert; check signature:
        payments::paymentrequest rcopy(paymentrequest); // copy
        rcopy.set_signature(std::string(""));
        std::string data_to_verify;                     // everything but the signature
        rcopy.serializetostring(&data_to_verify);

        evp_md_ctx ctx;
        evp_pkey *pubkey = x509_get_pubkey(signing_cert);
        evp_md_ctx_init(&ctx);
        if (!evp_verifyinit_ex(&ctx, digestalgorithm, null) ||
            !evp_verifyupdate(&ctx, data_to_verify.data(), data_to_verify.size()) ||
            !evp_verifyfinal(&ctx, (const unsigned char*)paymentrequest.signature().data(), (unsigned int)paymentrequest.signature().size(), pubkey)) {
            throw sslverifyerror("bad signature, invalid payment request.");
        }

        // openssl api for getting human printable strings from certs is baroque.
        int textlen = x509_name_get_text_by_nid(certname, nid_commonname, null, 0);
        website = new char[textlen + 1];
        if (x509_name_get_text_by_nid(certname, nid_commonname, website, textlen + 1) == textlen && textlen > 0) {
            merchant = website;
        }
        else {
            throw sslverifyerror("bad certificate, missing common name.");
        }
        // todo: detect ev certificates and set merchant = business name instead of unfriendly nid_commonname ?
    }
    catch (const sslverifyerror& err) {
        fresult = false;
        qwarning() << "paymentrequestplus::getmerchant: ssl error: " << err.what();
    }

    if (website)
        delete[] website;
    x509_store_ctx_free(store_ctx);
    for (unsigned int i = 0; i < certs.size(); i++)
        x509_free(certs[i]);

    return fresult;
}

qlist<std::pair<cscript,camount> > paymentrequestplus::getpayto() const
{
    qlist<std::pair<cscript,camount> > result;
    for (int i = 0; i < details.outputs_size(); i++)
    {
        const unsigned char* scriptstr = (const unsigned char*)details.outputs(i).script().data();
        cscript s(scriptstr, scriptstr+details.outputs(i).script().size());

        result.append(make_pair(s, details.outputs(i).amount()));
    }
    return result;
}
