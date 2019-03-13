class TransUnit{
   public:
      double amount;
      double timeSent;  //time delay after start time of simulation that trans-unit is active
      int sender;
      int receiver;
      //vector<int> route; //includes sender and reciever as first and last entries
      int priorityClass;
      bool hasTimeOut;
      double timeOut;
      double largerTxnId;


      TransUnit(double amount1, double timeSent1, int sender1, int receiver1, int priorityClass1, bool hasTimeOut1, double timeOut1 = -1, double largerTxnId1 = -1){
         assert((hasTimeOut1 && timeOut1>0) || (!(hasTimeOut1) && timeOut1==-1));
         amount = amount1;
         timeSent = timeSent1;
         sender = sender1;
         receiver = receiver1;
         priorityClass=  priorityClass1;
         hasTimeOut =  true; // TODO: temp, original: hasTimeOut1;
         timeOut = 5.0; // TODO: temp, original: timeOut1;
         largerTxnId = largerTxnId1;
      }
};
