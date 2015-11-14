#include "org_moorecoin_nativesecp256k1.h"
#include "include/secp256k1.h"

jniexport jint jnicall java_org_moorecoin_nativesecp256k1_secp256k1_1ecdsa_1verify
  (jnienv* env, jclass classobject, jobject bytebufferobject)
{
	unsigned char* data = (unsigned char*) (*env)->getdirectbufferaddress(env, bytebufferobject);
	int siglen = *((int*)(data + 32));
	int publen = *((int*)(data + 32 + 4));

	return secp256k1_ecdsa_verify(data, 32, data+32+8, siglen, data+32+8+siglen, publen);
}

static void __javasecp256k1_attach(void) __attribute__((constructor));
static void __javasecp256k1_detach(void) __attribute__((destructor));

static void __javasecp256k1_attach(void) {
	secp256k1_start(secp256k1_start_verify);
}

static void __javasecp256k1_detach(void) {
	secp256k1_stop();
}
