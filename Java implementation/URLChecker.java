/****************************************
 *           name: Omer Stein           *
 *  Url checker using multiple threads  *
 *  and synchronization, using a        *
 *  blocking queue implementation       *
 ***************************************/

import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.concurrent.BlockingQueue;

public class URLChecker implements Runnable {
    private final static int CONNECTION_TIMEOUT = 2000;
    private final static int READ_TIMEOUT = 2000;

    private final BlockingQueue<String> urlQueue;
    private final CheckResults globalResults;

    URLChecker(BlockingQueue<String> urlQueue, CheckResults globalResults) {
        this.urlQueue = urlQueue;
        this.globalResults = globalResults;
    }

    @Override
    public void run() {
        /*
        loop: pop URL from the urlQueue, if not empty- check it, otherwise break from the loop
        make sure to update globalResults (you decide when and how)
         */

    	//initializing parameters / counters
    	int ok = 0;
    	int error = 0; 
    	int unknown = 0;
    	String url = "";
  
    	try {
    		
    		//worker keeps taking (blocked if empty)
    		//from queue untill reaches empty string
    		while ((url = this.urlQueue.take()) != "" ){
    			//incrementing thread's params (not shared) 
    			//depends on checkURL return value with the given url
    			switch (checkURL(url)) {
    			case OK:
    				ok += 1;
    				break;
    			case ERROR:
    				error += 1;
    				break;
    			default:
    				unknown += 1;
    			}
    		}
        	//when finished, thread sends its data to globalResult
    		this.globalResults.threadUpdate(ok, error, unknown);	
    		
    	}
    	catch (Exception e){
    		System.err.println(e.toString());
    	}
    	
    }

    private static URLStatus checkURL(String url) {
        URLStatus status;

        try {
            HttpURLConnection connection = (HttpURLConnection) new URL(url).openConnection();
            connection.setConnectTimeout(CONNECTION_TIMEOUT);
            connection.setReadTimeout(READ_TIMEOUT);
            connection.setRequestMethod("HEAD");
            int responseCode = connection.getResponseCode();
            if (responseCode >= 200 && responseCode < 400) {
                status = URLStatus.OK;
            } else {
                status = URLStatus.ERROR;
            }
        } catch (IOException e) {
            status = URLStatus.UNKNOWN;
        }

        return status;
    }
}
