public class Sample {
	public static void main(String[] args) throws Exception {
		System.out.println("test jvm load by c");
        for (int i = 0; i < 20; i++) {
            byte[] bytes = new byte[1 << 20];
        }
		String slogLevel = System.getProperty("slog.level");
        System.out.println(slogLevel);
		Thread.sleep(5 * 1000);
    }
}