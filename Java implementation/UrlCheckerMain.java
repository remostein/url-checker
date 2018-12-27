/****************************************
 * 	 		 name: Omer Stein  		    *
 *  Url checker using multiple threads  *
 *  and synchronization, using a 		*
 *  blocking queue implementation 		*
 ***************************************/


import java.util.concurrent.*;

public class UrlCheckerMain {
    private final static int QUEUE_CAPACITY = 16;

    private static void CheckURLsFromFile(String filename, int numberOfWorkers) {
        /*  GOALS:
        1. Initialize a BlockingQueue implementation with limited capacity (QUEUE_CAPACITY)
        2. Start a URLFileReader thread
        3. Start numberOfWorkers URLChecker threads
        4. Join URLFileReader thread
        5. Join URLChecker threads
         */
        CheckResults results = new CheckResults();		
        
        //Creating Blocking Queue
        BlockingQueue<String> urlQueue = new ArrayBlockingQueue<String>(QUEUE_CAPACITY);
        //Creating URL file Reader
        Thread fileReader = new Thread(new URLFileReader(filename, urlQueue, numberOfWorkers));
        fileReader.start();
        
        //Creating thread array 
        Thread[] workers = new Thread[numberOfWorkers];
        //Creating and starting all the threads in the Thread array
        for (int i = 0; i < numberOfWorkers; i++){
        	workers[i] = new Thread(new URLChecker(urlQueue, results));
        	workers[i].start();
        }
        
        //Putting the thread joining in try / catch
        try {
        	fileReader.join();
        	for (int i = 0; i < numberOfWorkers; i++){
        		workers[i].join();
        	}
        }
        catch (Exception e) {
        	System.err.println(e.toString());
        	
        }   
        
        results.print();
    }

    public static void main(String[] args) {
        if (args.length != 2) {
            System.err.println("usage: java UrlCheckerMain URLS_FILENAME NUMBER_OF_WORKERS");
        } else {
            String filename = args[0];
            int numberOfWorkers = Integer.valueOf(args[1]);

            CheckURLsFromFile(filename, numberOfWorkers);
        }
    }
}
