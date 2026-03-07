/**
 * The queue is from: 
 *   https://stackoverflow.com/questions/61357982/how-could-i-quit-a-c-blocking-queue
 */

template <typename T> class Worker {
  int mId;
  bool mShutdown = false;

  std::mutex mtx;
  std::condition_variable not_full;
  std::condition_variable not_empty;
  std::queue<T> queue;
  size_t capacity{100};

public:

  Worker() : mId() {}
  Worker(int id) {setId(id);}
  virtual ~Worker() {}


  inline void setId(int id) {
    // TODO validation of id?
    printf("i'm worker %d\n",id);
    mId = id;
  }

  inline void shutdown() {
     mShutdown = true;
     not_empty.notify_all();
  }

  inline void addWork(const T& item)
  {
     if ( mShutdown ) return;

     //printf("%03d: got work\n",mId);
     std::unique_lock<std::mutex> lock(mtx);
     while (queue.size() >= capacity) {
            not_full.wait(lock, [&]{return queue.size() < capacity;});
     }
     queue.push(item);
     not_empty.notify_all();
  }

  inline T take() {
     if ( mShutdown ) {
        // TODO find a better choice.
        throw std::runtime_error("queue is shutdown");
     }

     std::unique_lock<std::mutex> lock(mtx);
     while (queue.empty()) {
            not_empty.wait(lock, [&]{return !queue.empty();});
     }
     T res = queue.front();
     queue.pop();
     not_full.notify_all();
     //return res;
     return std::move(res);
  }

  inline bool empty() {
     std::unique_lock<std::mutex> lock(mtx);
     return queue.empty();
  }

  inline size_t size() {
     std::unique_lock<std::mutex> lock(mtx);
     return queue.size();
  }

  inline void set_capacity(const size_t capacity) {
     this->capacity = (capacity > 0 ? capacity : 10);
  }

};

