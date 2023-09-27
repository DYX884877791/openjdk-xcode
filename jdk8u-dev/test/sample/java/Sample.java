public class Sample {
	public static void main(String[] args) throws Exception {
		System.out.println("test jvm load by c");
		 String slogLevel = System.getProperty("slog.level");
         System.out.println(slogLevel);
		 Thread.sleep(6000 * 1000);
    }
}