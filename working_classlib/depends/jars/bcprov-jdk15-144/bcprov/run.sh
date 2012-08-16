SOOT_PATH=/home/dennis/tool/Soot-trunk-for-opt
#JAVA_LIB_PATH=/home/dennis/JDK1.5/jdk1.5.0_22/jre/lib
JAVA_LIB_PATH=/home/dennis/jdk1.6.0_23/jre/lib


java -ea -esa -Xms512m -Xmx10000m -cp $SOOT_PATH/soot-dev/lib/soot-trunk.jar soot.Main -w -p cg.spark enabled:true -p wjop off -p wjtp off -p wjap.tlot enabled:true,print-debug:true -p tag.tlo enabled:true -main-class org.bouncycastle.crypto.examples.RSAExample -i spec -cp .:$JAVA_LIB_PATH/rt.jar:$JAVA_LIB_PATH/jce.jar org.bouncycastle.crypto.examples.RSAExample org.bouncycastle.crypto.engines.RSAEngine | tee soot.out.txt

#java -ea -esa -Xms512m -Xmx10000m -cp $SOOT_PATH/soot-dev/lib/soot-trunk.jar soot.Main -w -p cg.spark enabled:true -p wjop off -p wjtp off -p wjap.tlot enabled:true,print-debug:true -p tag.tlo enabled:true -main-class org.bouncycastle.crypto.examples.DESRawExample -i spec -cp .:$JAVA_LIB_PATH/rt.jar:$JAVA_LIB_PATH/jce.jar org.bouncycastle.crypto.examples.DESRawExample org.bouncycastle.crypto.engines.DESEngine | tee soot.out.txt

#java -ea -esa -Xms512m -Xmx10000m -cp $SOOT_PATH/soot-dev/lib/soot-trunk.jar soot.Main -w -p cg.spark enabled:true -p wjop off -p wjtp off -p wjap.tlot enabled:true,print-debug:true -p tag.tlo enabled:true -main-class org.bouncycastle.crypto.examples.AESFastExample -i spec -cp .:$JAVA_LIB_PATH/rt.jar:$JAVA_LIB_PATH/jce.jar org.bouncycastle.crypto.examples.AESFastExample org.bouncycastle.crypto.engines.AESFastEngine | tee soot.out.txt


