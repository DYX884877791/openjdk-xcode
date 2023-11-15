public class FullGC {
	public static void main(String[] args) throws Exception {
		System.out.println("full gc test");
        for (int i = 0; i < 20; i++) {
            byte[] bytes = new byte[1 << 20];
        }
    }
}