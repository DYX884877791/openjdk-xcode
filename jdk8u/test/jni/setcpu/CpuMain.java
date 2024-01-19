public class CpuMain {
    public static void main(String[] args) {
        System.out.println(SetCpu.getCpu(new Runnable() {

            @Override
            public void run() {
                System.out.println("cpu");
            }
        }));
    }
}
