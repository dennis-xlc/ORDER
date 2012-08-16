// Decompiled by Jad v1.5.8e. Copyright 2001 Pavel Kouznetsov.
// Jad home page: http://www.geocities.com/kpdus/jad.html
// Decompiler options: packimports(3) 

package org.bouncycastle.crypto.examples;

import java.io.*;
import java.security.SecureRandom;
import org.bouncycastle.crypto.CryptoException;
import org.bouncycastle.crypto.KeyGenerationParameters;
import org.bouncycastle.crypto.engines.RSAEngine;
import org.bouncycastle.crypto.generators.DESKeyGenerator;
import org.bouncycastle.crypto.modes.CBCBlockCipher;
import org.bouncycastle.crypto.paddings.PaddedBufferedBlockCipher;
import org.bouncycastle.crypto.params.KeyParameter;
import org.bouncycastle.util.encoders.Hex;

public class RSAExample
{

    public static void main(String args[])
    {
        boolean flag = true;
        String s = null;
        String s1 = null;
        String s2 = null;
        if(args.length < 2)
        {
            RSAExample desexample = new RSAExample();
            System.err.println((new StringBuilder()).append("Usage: java ").append(desexample.getClass().getName()).append(" infile outfile [keyfile]").toString());
            System.exit(1);
        }
        s2 = "deskey.dat";
        s = args[0];
        s1 = args[1];
        if(args.length > 2)
        {
            flag = false;
            s2 = args[2];
        }
        RSAExample desexample1 = new RSAExample(s, s1, s2, flag);
        desexample1.process();
    }

    public RSAExample()
    {
        encrypt = true;
        cipher = null;
        in = null;
        out = null;
        key = null;
    }

    public RSAExample(String s, String s1, String s2, boolean flag)
    {
        encrypt = true;
        cipher = null;
        in = null;
        out = null;
        key = null;
        encrypt = flag;
        try
        {
            in = new BufferedInputStream(new FileInputStream(s));
        }
        catch(FileNotFoundException filenotfoundexception)
        {
            System.err.println((new StringBuilder()).append("Input file not found [").append(s).append("]").toString());
            System.exit(1);
        }
        try
        {
            out = new BufferedOutputStream(new FileOutputStream(s1));
        }
        catch(IOException ioexception)
        {
            System.err.println((new StringBuilder()).append("Output file not created [").append(s1).append("]").toString());
            System.exit(1);
        }
        if(flag)
            try
            {
                SecureRandom securerandom = null;
                try
                {
                    securerandom = new SecureRandom();
                    securerandom.setSeed("www.bouncycastle.org".getBytes());
                }
                catch(Exception exception)
                {
                    System.err.println("Hmmm, no SHA1PRNG, you need the Sun implementation");
                    System.exit(1);
                }
                KeyGenerationParameters keygenerationparameters = new KeyGenerationParameters(securerandom, 192);
                DESKeyGenerator desedekeygenerator = new DESKeyGenerator();
                desedekeygenerator.init(keygenerationparameters);
                key = desedekeygenerator.generateKey();
                BufferedOutputStream bufferedoutputstream = new BufferedOutputStream(new FileOutputStream(s2));
                byte abyte1[] = Hex.encode(key);
                bufferedoutputstream.write(abyte1, 0, abyte1.length);
                bufferedoutputstream.flush();
                bufferedoutputstream.close();
            }
            catch(IOException ioexception1)
            {
                System.err.println((new StringBuilder()).append("Could not decryption create key file [").append(s2).append("]").toString());
                System.exit(1);
            }
        else
            try
            {
                BufferedInputStream bufferedinputstream = new BufferedInputStream(new FileInputStream(s2));
                int i = bufferedinputstream.available();
                byte abyte0[] = new byte[i];
                bufferedinputstream.read(abyte0, 0, i);
                key = Hex.decode(abyte0);
            }
            catch(IOException ioexception2)
            {
                System.err.println((new StringBuilder()).append("Decryption key file not found, or not valid [").append(s2).append("]").toString());
                System.exit(1);
            }
    }

    private void process()
    {
        cipher = new PaddedBufferedBlockCipher(new CBCBlockCipher(new RSAEngine()));
        if(encrypt)
            performEncrypt(key);
        else
            performDecrypt(key);
        try
        {
            in.close();
            out.flush();
            out.close();
        }
        catch(IOException ioexception) { }
    }

    private void performEncrypt(byte abyte0[])
    {
        cipher.init(true, new KeyParameter(abyte0));
        byte byte0 = 47;
        int i = cipher.getOutputSize(byte0);
        byte abyte1[] = new byte[byte0];
        byte abyte2[] = new byte[i];
        try
        {
            Object obj = null;
            do
            {
                int j;
                if((j = in.read(abyte1, 0, byte0)) <= 0)
                    break;
                int k = cipher.processBytes(abyte1, 0, j, abyte2, 0);
                if(k > 0)
                {
                    byte abyte3[] = Hex.encode(abyte2, 0, k);
                    out.write(abyte3, 0, abyte3.length);
                    out.write(10);
                }
            } while(true);
            try
            {
                int l = cipher.doFinal(abyte2, 0);
                if(l > 0)
                {
                    byte abyte4[] = Hex.encode(abyte2, 0, l);
                    out.write(abyte4, 0, abyte4.length);
                    out.write(10);
                }
            }
            catch(CryptoException cryptoexception) { }
        }
        catch(IOException ioexception)
        {
            ioexception.printStackTrace();
        }
    }

    private void performDecrypt(byte abyte0[])
    {
        cipher.init(false, new KeyParameter(abyte0));
        BufferedReader bufferedreader = new BufferedReader(new InputStreamReader(in));
        try
        {
            Object obj = null;
            byte abyte2[] = null;
            Object obj1 = null;
            do
            {
                String s;
                if((s = bufferedreader.readLine()) == null)
                    break;
                byte abyte1[] = Hex.decode(s);
                abyte2 = new byte[cipher.getOutputSize(abyte1.length)];
                int i = cipher.processBytes(abyte1, 0, abyte1.length, abyte2, 0);
                if(i > 0)
                    out.write(abyte2, 0, i);
            } while(true);
            try
            {
                int j = cipher.doFinal(abyte2, 0);
                if(j > 0)
                    out.write(abyte2, 0, j);
            }
            catch(CryptoException cryptoexception) { }
        }
        catch(IOException ioexception)
        {
            ioexception.printStackTrace();
        }
    }

    private boolean encrypt;
    private PaddedBufferedBlockCipher cipher;
    private BufferedInputStream in;
    private BufferedOutputStream out;
    private byte key[];
}
