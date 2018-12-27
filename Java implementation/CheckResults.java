/****************************************
 *           name: Omer Stein           *
 *  Url checker using multiple threads  *
 *  and synchronization, using a        *
 *  blocking queue implementation       *
 ***************************************/

class CheckResults {
    /*
    Implement a way/ways to set/update the ok/error/unknown values. Handle mutual exclusion where needed.
     */
    private int ok = 0;
    private int error = 0;
    private int unknown = 0;

    //using synchronized threadUpdate to update globalResult individually (like with mutex)
    public synchronized void threadUpdate (int i_ok, int i_error, int i_unknown){
    	this.ok += i_ok;
    	this.error += i_error;
    	this.unknown += i_unknown;
    }

    void print() {
        System.out.printf("%d OK, %d Error, %d Unknown\n",
                ok,
                error,
                unknown);
    }
}
