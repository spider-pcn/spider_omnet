class transUnit{
   public:
      double amount;
      double timeSent;  //time delay after start time of simulation that trans-unit is active
      int sender;
      int receiver;
      //vector<int> route; //includes sender and reciever as first and last entries
      int priorityClass;

      transUnit(double amount1, double timeSent1, int sender1, int receiver1, int priorityClass1){
         amount = amount1;
         timeSent = timeSent1;
         sender = sender1;
         receiver = receiver1;
         priorityClass=  priorityClass1;
      }
};
