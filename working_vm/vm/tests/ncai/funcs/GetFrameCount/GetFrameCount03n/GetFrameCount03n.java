package ncai.funcs;

/**
 * @author Petr Ivanov
 *
 */
public class GetFrameCount03n extends Thread{
    public native void TestFunction();
    public native void TestFunction1();
    public static native boolean stopsignal();
    public static native void resumeagent();

    static boolean NoLibrary = false;
    static {
        try{
            System.loadLibrary("GetFrameCount03n");
        }
        catch(Throwable e){
            NoLibrary = true;
        }
    }

    GetFrameCount03n(String name)
    {
        super(name);
    }

    static public void main(String args[]) {
        if(NoLibrary) return;
        new GetFrameCount03n("java_thread").start();
        special_method();
        /*
        while(!stopsignal()){
            try {
                sleep(100, 0); // milliseconds
            } catch (java.lang.InterruptedException ie) {}
            System.out.println("\tJAVA: main: ...");
        }

        try {
            sleep(1000, 0); // milliseconds
        } catch (java.lang.InterruptedException ie) {}

        System.out.println("\tJAVA: main - exit");

*/
        return;
    }

    public void test_java_func1(){
        System.out.println("thread - java func1\n");
        TestFunction();
    }

    public void test_java_func2(){
        System.out.println("thread - java func2\n");
        test_java_func3();
    }

    public void test_java_func3(){
        System.out.println("thread - java func3\n");
        TestFunction1();
        resumeagent();  /*
        while(!stopsignal())
        {
            try {
                sleep(100, 0); // milliseconds
            } catch (java.lang.InterruptedException ie) {}
            //System.out.println("\tJAVA: Thread: ...");
        }*/
    }

    static public void special_method() {
        /*
         * Transfer control to native part.
         */
        try {
            throw new InterruptedException();
        } catch (Throwable tex) { }
        return;
    }

    public void run() {
        System.out.println("thread - java run\n");
        test_java_func1();
    }
}


