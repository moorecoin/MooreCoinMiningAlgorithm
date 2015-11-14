package org.moorecoin;

import java.nio.bytebuffer;
import java.nio.byteorder;

import com.google.common.base.preconditions;


/**
 * this class holds native methods to handle ecdsa verification.
 * you can find an example library that can be used for this at
 * https://github.com/sipa/secp256k1
 */
public class nativesecp256k1 {
    public static final boolean enabled;
    static {
        boolean isenabled = true;
        try {
            system.loadlibrary("javasecp256k1");
        } catch (unsatisfiedlinkerror e) {
            isenabled = false;
        }
        enabled = isenabled;
    }
    
    private static threadlocal<bytebuffer> nativeecdsabuffer = new threadlocal<bytebuffer>();
    /**
     * verifies the given secp256k1 signature in native code.
     * calling when enabled == false is undefined (probably library not loaded)
     * 
     * @param data the data which was signed, must be exactly 32 bytes
     * @param signature the signature
     * @param pub the public key which did the signing
     */
    public static boolean verify(byte[] data, byte[] signature, byte[] pub) {
        preconditions.checkargument(data.length == 32 && signature.length <= 520 && pub.length <= 520);

        bytebuffer bytebuff = nativeecdsabuffer.get();
        if (bytebuff == null) {
            bytebuff = bytebuffer.allocatedirect(32 + 8 + 520 + 520);
            bytebuff.order(byteorder.nativeorder());
            nativeecdsabuffer.set(bytebuff);
        }
        bytebuff.rewind();
        bytebuff.put(data);
        bytebuff.putint(signature.length);
        bytebuff.putint(pub.length);
        bytebuff.put(signature);
        bytebuff.put(pub);
        return secp256k1_ecdsa_verify(bytebuff) == 1;
    }

    /**
     * @param bytebuff signature format is byte[32] data,
     *        native-endian int signaturelength, native-endian int pubkeylength,
     *        byte[signaturelength] signature, byte[pubkeylength] pub
     * @returns 1 for valid signature, anything else for invalid
     */
    private static native int secp256k1_ecdsa_verify(bytebuffer bytebuff);
}
