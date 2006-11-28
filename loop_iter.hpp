template <int C, class ArgT>
class Storage {
  typedef typename ArgInType<ArgT>::Result store_t;
  store_t t_;
protected:
  Storage(IN(ArgT) t) : t_(t) {;}
  store_t& value() { return t_; }
};

/* This is a bad idea if qthreads get stack references confused... I think...
template <int C, class T>
class Storage <C, Ref<T> > {
  T& tr_;
protected:
  Storage(IN(Ref<T>) t) : tr_(t) {;}
  T& value() { return tr_; }
};
*/

template <int C, class T>
class Storage <C, Ref<T> > {
  T* tp_;
protected:
  Storage(IN(Ref<T>) t) : tp_(&t) {;}
  T& value() { return *tp_; }
};

template <int C>
class Storage<C,Iterator> {
protected:
  Storage(Iterator* v) {;}
  static Iterator *value() { return NULL; } 
};

template <int C>
class Storage<C,void> {
protected:
  Storage(void* v) {;}
  static void *value() { return NULL; } 
};

#define CALL_5_ARG(fff) fff(Value<Arg1T>::value(Storage<1, Arg1T>::value(),cur), \
			    Value<Arg2T>::value(Storage<2, Arg2T>::value(),cur), \
			    Value<Arg3T>::value(Storage<3, Arg3T>::value(),cur), \
			    Value<Arg4T>::value(Storage<4, Arg4T>::value(),cur), \
			    Value<Arg5T>::value(Storage<5, Arg5T>::value(),cur))

#define CALL_4_ARG(fff) fff(Value<Arg1T>::value(Storage<1, Arg1T>::value(),cur), \
			    Value<Arg2T>::value(Storage<2, Arg2T>::value(),cur), \
			    Value<Arg3T>::value(Storage<3, Arg3T>::value(),cur), \
			    Value<Arg4T>::value(Storage<4, Arg4T>::value(),cur))

#define CALL_3_ARG(fff) fff(Value<Arg1T>::value(Storage<1, Arg1T>::value(),cur), \
			    Value<Arg2T>::value(Storage<2, Arg2T>::value(),cur), \
			    Value<Arg3T>::value(Storage<3, Arg3T>::value(),cur))

#define CALL_2_ARG(fff) fff(Value<Arg1T>::value(Storage<1, Arg1T>::value(),cur), \
			    Value<Arg2T>::value(Storage<2, Arg2T>::value(),cur))

#define CALL_1_ARG(fff) fff(Value<Arg1T>::value(Storage<1, Arg1T>::value(),cur))

#define CALL_0_ARG(fff) fff()

#define CALL_N_ARG(fff,nnn) CALL_##nnn##_ARG(fff)

#define C_RUN_OBJ(nnn)							\
  void run (int cur) {							\
    if (Storage<0, RetT*>::value() != NULL)				\
      Storage<0, RetT*>::value()[cur] =					\
	CALL_N_ARG( (Storage<-1,ObjT*>::value()->*(fptr_)), nnn);	\
    else								\
      CALL_N_ARG( (Storage<-1,ObjT*>::value()->*(fptr_)), nnn);		\
  }									

  
#define C_RUN(nnn)							\
  void run (int cur) {							\
    if (Storage<0, RetT*>::value() != NULL)				\
      Storage<0, RetT*>::value()[cur] = CALL_N_ARG(fptr_,nnn);	\
    else								\
      CALL_N_ARG(fptr_, nnn);						\
  }									


#define C_VOID_RUN_OBJ(nnn)				\
  void run (int cur) {					\
    ObjT *obj = Storage<-1,ObjT*>::value();		\
    CALL_N_ARG((obj->*(fptr_)), nnn);			\
  }							


#define C_VOID_RUN(nnn)					\
  void run (int cur) {					\
    CALL_N_ARG(fptr_, nnn);				\
  }							

#define C_CLOSE() };

template <class FptrT, class ObjT, class RetT, 
	  class Arg1T, class Arg2T, class Arg3T,
	  class Arg4T, class Arg5T>
class RunFunc : public Storage<-1,ObjT*>,
		public Storage<0, RetT*>,
		public Storage<1, Arg1T>,
		public Storage<2, Arg2T>,
		public Storage<3, Arg3T>,
		public Storage<4, Arg4T>,
		public Storage<5, Arg5T>

{
  FptrT fptr_;  
protected:
  RunFunc (FptrT fptr, ObjT *obj, RetT *ret, 
	   IN(Arg1T) arg1, IN(Arg2T) arg2, IN(Arg3T) arg3,
	   IN(Arg4T) arg4, IN(Arg5T) arg5) :
    fptr_(fptr), 
    Storage<-1,ObjT*>(obj), Storage<0,RetT*>(ret),
    Storage<1,Arg1T>(arg1), Storage<2,Arg2T>(arg2),
    Storage<3,Arg3T>(arg3), Storage<4,Arg4T>(arg4),
    Storage<5,Arg5T>(arg5) {;}

  C_RUN_OBJ(5);
};

#define C_BODY(ooo,rrr,a1,a2,a3,a4,a5)					\
  class RunFunc<FptrT,ooo,rrr,a1,a2,a3,a4,a5> : public Storage<-1,ooo*>, \
						public Storage<0, rrr*>, \
						public Storage<1, a1>,	\
						public Storage<2, a2>,	\
						public Storage<3, a3>,	\
						public Storage<4, a4>,	\
						public Storage<5, a5>	\
{									\
  FptrT fptr_;								\
 protected:								\
  RunFunc (FptrT fptr, ooo *obj, rrr *ret,				\
	   IN(a1) arg1, IN(a2) arg2, IN(a3) arg3,			\
	   IN(a4) arg4, IN(a5) arg5) :					\
    fptr_(fptr),							\
    Storage<-1,ooo*>(obj), Storage<0,rrr*>(ret),			\
    Storage<1,a1>(arg1), Storage<2,a2>(arg2),				\
    Storage<3,a3>(arg3), Storage<4,a4>(arg4),				\
    Storage<5,a5>(arg5) {;}						



template <class FptrT, class ObjT, class Arg1T, class Arg2T, class Arg3T, class Arg4T, class Arg5T>
C_BODY(ObjT,void,Arg1T,Arg2T,Arg3T,Arg4T,Arg5T);
C_VOID_RUN_OBJ(5);
C_CLOSE();

template <class FptrT, class RetT, class Arg1T, class Arg2T, class Arg3T, class Arg4T, class Arg5T>
C_BODY(void,RetT,Arg1T,Arg2T,Arg3T,Arg4T,Arg5T);
C_RUN(5);
C_CLOSE();

template <class FptrT, class Arg1T, class Arg2T, class Arg3T, class Arg4T, class Arg5T>
C_BODY(void,void,Arg1T,Arg2T,Arg3T,Arg4T,Arg5T);
C_VOID_RUN(5);
C_CLOSE();

/** 4 Args **/

template <class FptrT, class ObjT, class RetT, class Arg1T, class Arg2T, class Arg3T, class Arg4T>
C_BODY(ObjT,RetT,Arg1T,Arg2T,Arg3T,Arg4T,void);
C_RUN_OBJ(4);
C_CLOSE();

template <class FptrT, class ObjT, class Arg1T, class Arg2T, class Arg3T, class Arg4T>
C_BODY(ObjT,void,Arg1T,Arg2T,Arg3T,Arg4T,void);
C_VOID_RUN_OBJ(4);
C_CLOSE();

template <class FptrT, class RetT, class Arg1T, class Arg2T, class Arg3T, class Arg4T>
C_BODY(void,RetT,Arg1T,Arg2T,Arg3T,Arg4T,void);
C_RUN(4);
C_CLOSE();

template <class FptrT, class Arg1T, class Arg2T, class Arg3T, class Arg4T>
C_BODY(void,void,Arg1T,Arg2T,Arg3T,Arg4T,void);
C_VOID_RUN(4);
C_CLOSE();

/** 3 Args **/

template <class FptrT, class ObjT, class RetT, class Arg1T, class Arg2T, class Arg3T>
C_BODY(ObjT,RetT,Arg1T,Arg2T,Arg3T,void,void);
C_RUN_OBJ(3);
C_CLOSE();

template <class FptrT, class ObjT, class Arg1T, class Arg2T, class Arg3T>
C_BODY(ObjT,void,Arg1T,Arg2T,Arg3T,void,void);
C_VOID_RUN_OBJ(3);
C_CLOSE();

template <class FptrT, class RetT, class Arg1T, class Arg2T, class Arg3T>
C_BODY(void,RetT,Arg1T,Arg2T,Arg3T,void,void);
C_RUN(3);
C_CLOSE();

template <class FptrT, class Arg1T, class Arg2T, class Arg3T>
C_BODY(void,void,Arg1T,Arg2T,Arg3T,void,void);
C_VOID_RUN(3);
C_CLOSE();

/** 2 Args **/

template <class FptrT, class ObjT, class RetT, class Arg1T, class Arg2T>
C_BODY(ObjT,RetT,Arg1T,Arg2T,void,void,void);
C_RUN_OBJ(2);
C_CLOSE();

template <class FptrT, class ObjT, class Arg1T, class Arg2T>
C_BODY(ObjT,void,Arg1T,Arg2T,void,void,void);
C_VOID_RUN_OBJ(2);
C_CLOSE();

template <class FptrT, class RetT, class Arg1T, class Arg2T>
C_BODY(void,RetT,Arg1T,Arg2T,void,void,void);
C_RUN(2);
C_CLOSE();

template <class FptrT, class Arg1T, class Arg2T>
C_BODY(void,void,Arg1T,Arg2T,void,void,void);
C_VOID_RUN(2);
C_CLOSE();

/** 1 Args **/

template <class FptrT, class ObjT, class RetT, class Arg1T>
C_BODY(ObjT,RetT,Arg1T,void,void,void,void);
C_RUN_OBJ(1);
C_CLOSE();

template <class FptrT, class ObjT, class Arg1T>
C_BODY(ObjT,void,Arg1T,void,void,void,void);
C_VOID_RUN_OBJ(1);
C_CLOSE();

template <class FptrT, class RetT, class Arg1T>
C_BODY(void,RetT,Arg1T,void,void,void,void);
C_RUN(1);
C_CLOSE();

template <class FptrT, class Arg1T>
C_BODY(void,void,Arg1T,void,void,void,void);
C_VOID_RUN(1);
C_CLOSE();

/** 0 Args **/

template <class FptrT, class ObjT, class RetT>
C_BODY(ObjT,RetT,void,void,void,void,void);
C_RUN_OBJ(0);
C_CLOSE();

template <class FptrT, class ObjT>
C_BODY(ObjT,void,void,void,void,void,void);
C_VOID_RUN_OBJ(0);
C_CLOSE();

template <class FptrT, class RetT>
C_BODY(void,RetT,void,void,void,void,void);
C_RUN(0);
C_CLOSE();

template <class FptrT >
C_BODY(void,void,void,void,void,void,void);
C_VOID_RUN(0);
C_CLOSE();

/** Iteration Object **/

class Iter {
public:
  virtual int Count() = 0;
  virtual void Run() = 0;
  virtual void Next() = 0;
  virtual ~Iter() {;}
};

template <class ObjT, class RetT, class FptrT, class Arg1T, class Arg2T, 
	  class Arg3T, class Arg4T, class Arg5T>
class IterArg : public Iter, 
		public RunFunc< FptrT, ObjT,RetT,Arg1T,Arg2T,Arg3T,Arg4T,Arg5T >
{
  int cur_, count_, step_;

  typedef RunFunc< FptrT, ObjT,RetT,Arg1T,Arg2T,Arg3T,Arg4T,Arg5T > runner;
public:
  virtual void Run() { 
    runner::run( cur_ ); 
  }
  
  virtual void Next() { cur_ += step_; }
  virtual ~IterArg() {;}
  virtual int Count() { return count_; }

  IterArg(ObjT *obj, RetT *ret, FptrT fptr,
	  IN(Arg1T) arg1, IN(Arg2T) arg2, IN(Arg3T) arg3,
	  IN(Arg4T) arg4, IN(Arg5T) arg5,
	  int start, int step, int count) : 
    runner(fptr, obj, ret, arg1, arg2, arg3, arg4, arg5),
    cur_(start), step_(step), count_(count) {;}
    // { printf ("Created IterArg size %d\n", 
    //    sizeof(IterArg<ObjT,RetT,Arg1T,Arg2T,Arg3T,Arg4T,Arg5T>)); }
    
};

template <class IterT>
void run_ft (qthread_t *qthr, void *arg) {
  IterT* iter = (IterT*)arg;
  register int count = iter->Count();
  for (int i = 0; i < count; i++) {
    iter->Run();
    iter->Next();
  }
  delete iter;
  future_exit(qthr);
}

template <class IterT>
void run_qtd (qthread_t *qthr, void *arg) {
  IterT* iter = (IterT*)arg;
  register int count = iter->Count();

  for (int i = 0; i < count; i++) {
    iter->Run();
    iter->Next();
  }
  delete iter;
}

#undef CALL_5_ARG
#undef CALL_4_ARG
#undef CALL_3_ARG
#undef CALL_2_ARG
#undef CALL_1_ARG
#undef CALL_0_ARG
#undef CALL_N_ARG
#undef C_RUN_OBJ
#undef C_RUN
#undef C_VOID_RUN_OBJ
#undef C_VOID_RUN
#undef C_CLOSE
#undef C_BODY
