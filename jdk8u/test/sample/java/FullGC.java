import java.util.List;
import java.util.ArrayList;

/**
 * -Xmx50m -Xms50m -XX:-UseAdaptiveSizePolicy
 */
public class FullGC {

	private final static List<byte[]> list = new ArrayList();
	public static void main(String[] args) throws Exception {
		System.out.println("full gc test");
		int i = 0;
		while(true) {
		    byte[] bytes = new byte[1 << 20];
		    Thread.sleep(1000);
		    list.add(bytes);
		    System.out.println("i-->" + i);
		    i++;
		    if (i%20==0) {
		        list.clear();
		    }
		}
    }
}