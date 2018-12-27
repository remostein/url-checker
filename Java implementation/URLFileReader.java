/****************************************
 *           name: Omer Stein           *
 *  Url checker using multiple threads  *
 *  and synchronization, using a        *
 *  blocking queue implementation       *
 ***************************************/

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.concurrent.BlockingQueue;

public class URLFileReader implements Runnable {
    private String filename;
    private BlockingQueue<String> urlQueue;
    private int numberOfConsumers;

    URLFileReader(String filename, BlockingQueue<String> urlQueue, int numberOfConsumers) {
        this.filename = filename;
        this.urlQueue = urlQueue;
        this.numberOfConsumers = numberOfConsumers;
    }

    @Override
    public void run() {
        /*
        open filename, read lines and push them into urlQueue
        push numberOfConsumers empty strings into urlQueue
         */

    	//Initializing parameters
    	String line;
    	File urlFile;
    	BufferedReader br;
    	try {
    		    urlFile = new File(this.filename);
    		    br = new BufferedReader(new FileReader(urlFile));
    		    
    		    //read lines untill end of file and add them to urlQueue with put (that blocks if full)
    		    while ((line = br.readLine()) != null) {
    		    	this.urlQueue.put(line);
    		    }
    		    
    		    //insert empty strings at the end of the queue
    		    for (int i = 0; i < numberOfConsumers; i++){
    		    	this.urlQueue.put("");
    		    }
    		    
    		    br.close();
    	} 
    	catch (IOException e) {
    		System.err.println(e.toString());
    	}
    	//handles all exceptions
    	catch (Exception e){
    		System.err.println(e.toString());
    	}
    }
}
