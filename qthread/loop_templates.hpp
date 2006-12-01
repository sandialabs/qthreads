#ifndef FUTURE_TEMP_HPP
#define FUTURE_TEMP_HPP

class Iterator {
};

template <class PtrT>
class ArrayPtr {
};

template <class T>
class Ref {
};

template <class ArgT>
class Value {
public:
  static const ArgT& value(ArgT& arg, int& iteration) { return arg; }
};

template<>
class Value<Iterator> {
 public:
  static int value(void *arg, int& iteration) { return iteration; }
};

template<class PtrT>
class Value< ArrayPtr<PtrT> > {
public:
  static PtrT sample;
  typedef typeof (sample.operator[](0)) ele_t;
  static ele_t& value(PtrT arg, int& iteration) { return arg[iteration]; }
};

template<class T>
class Value< ArrayPtr<T*> > {
public:
  static T& value(T* arg, int& iteration) { return arg[iteration]; }
};

template<class T>
class Value< Ref<T> > {
public:
  static T& value( T& arg, int& iteration) { return arg; }
};

template <class T>
struct RemoveRef {
  typedef T Result;
};

template <class T>
struct RemoveRef<T&> {
  typedef T Result;
};

template <class ArgT>
struct ArgInType {
  typedef typename RemoveRef<ArgT>::Result ArgTT;
  typedef const ArgTT& RefResult;
  typedef ArgTT Result;
};

template <class T>
struct ArgInType < Ref<T> >{
  typedef typename RemoveRef<T>::Result TT;
  typedef TT& RefResult;
  typedef TT Result;
};

template<>
struct ArgInType<void> {
  typedef void* RefResult;
  typedef void* Result;
};

template<>
struct ArgInType< Iterator > {
  typedef Iterator* RefResult;
  typedef Iterator* Result;
};

template <class PtrT>
struct ArgInType< ArrayPtr<PtrT> > {
  typedef typename RemoveRef<PtrT>::Result PtrTT;
  typedef PtrTT& RefResult;
  typedef PtrTT Result;
};

#define IN(ttt) typename ArgInType< ttt >::RefResult

#include <qthread/loop_iter.hpp>

#undef DBprintf
#if 0
#include <stdio.h>
#define DBprintf printf
#else
#define DBprintf(...) ;
#endif

namespace loop {
  const int Par = 1;
  const int ParNoJoin = 2;
  const int Future = 3;
  const int FutureNoJoin = 4;
};

#define ITER(start__,step__,count__) \
  new IterArg<ObjT,RetT,FptrT,Arg1T,Arg2T,Arg3T,Arg4T,Arg5T>		\
  (obj,ret,fptr,arg1,arg2,arg3,arg4,arg5,start__,step__,count__)

#define SCALE_TD_POW2(iterc,pow2) \
  if (iterc < 100)		   \
    pow2 = 0;			   \
  else if (iterc < 1000)	   \
    pow2 = 3;			   \
  else if (iterc < 5000)	   \
    pow2 = 5;			   \
  else if (iterc < 25000)	   \
    pow2 = 6;			   \
  else if (iterc < 100000)	   \
    pow2 = 7;			   \
  else if (iterc < 250000)	   \
    pow2 = 8;			   \
  else				   \
    pow2 = 9;


template <class ObjT, class RetT, class Arg1T, class Arg2T, class Arg3T, 
	  class Arg4T, class Arg5T, int TypeC, class FptrT>

void ParMemberLoop (ObjT *obj, RetT *ret,
		    FptrT fptr,
		    IN(Arg1T) arg1, IN(Arg2T) arg2, IN(Arg3T) arg3, 
		    IN(Arg4T) arg4, IN(Arg5T) arg5, 
		    int start, int stop, int step=1) {

  qthread_t *me = qthread_self();  
  bool join = true;

  int total, steptd, tdc, tdc_pow2, round_total, base_count;
  
  if (step == 1)
    total = (stop - start);
  else {
    total = (stop - start) / step;
    if (((stop - start) % step) != 0)
      total++;
  }

  SCALE_TD_POW2(total,tdc_pow2);

  tdc = 1 << tdc_pow2;
  steptd = step << tdc_pow2;
  base_count = total >> tdc_pow2;
  round_total = base_count << tdc_pow2;

  DBprintf ("Given start %d stop %d step %d: \n", start, stop, step);
  DBprintf ("Total is %d, tdc_pow2 is %d (tdc %d)\n", total, tdc_pow2, 1<<tdc_pow2);
  DBprintf ("Tdc is %d steptd is %d round_total %d base_count %d\n", 
	  tdc, steptd, round_total, base_count);

  switch (TypeC) {
  case loop::ParNoJoin:
    join = false;
  case loop::Par: {
    qthread_t **thr = new qthread_t*[tdc];
    for (int i = 0; i < tdc; i++) {
      int count = base_count + ( ((round_total + i) < total) ? 1 : 0 );
      thr[i] = qthread_fork(run_qtd<Iter>, ITER(start, steptd, count));

      DBprintf ("Thread %d %p start %d step %d count %d\n", 
		i, thr[i], start, steptd, count);
      start += step;
    }
    
    if (join) {
      for (int i = 0; i < tdc; i++)
	qthread_join(me, thr[i]);
    }
    delete thr;
  } break;
    
  case loop::FutureNoJoin:
    join = false;
  case loop::Future: {
    future_t **ft = new future_t*[total];
    int yielded = future_yield(me);

    for (int i = 0; i < tdc; i++) {
      int count = base_count + ( ((round_total + i) < total) ? 1 : 0 );
      ft[i] = future_create (me, run_ft<Iter>, ITER(start,steptd,count));
      start += step;
    }

    if (join)
      future_join_all (me, ft, tdc);
    if (yielded)
      future_acquire(me);

    delete ft;
  } break;

  }
}

//C_LIST = class list
//T_LIST = type list
//A_LIST = args list
//P_LIST = params list

#define PAR_LOOP()							\
  template <class RetT, C_LIST int TypeC, class FptrT>			\
  void ParLoop ( RetT *ret,						\
		 FptrT fptr,						\
		 A_LIST							\
		 int start, int stop, int step = 1 ) {			\
    ParMemberLoop <void, RetT, T_LIST, TypeC, FptrT>			\
      (NULL, ret, fptr, P_LIST, start, stop, step);			\
  }									

#define PAR_VOID_LOOP()							\
  template <C_LIST int TypeC, class FptrT>				\
  void ParVoidLoop ( FptrT fptr,					\
		     A_LIST						\
		     int start, int stop, int step = 1 ) {		\
    ParMemberLoop <void, void, T_LIST, TypeC, FptrT>			\
      (NULL, NULL, fptr, P_LIST, start, stop, step);			\
  }

#define PAR_MEMBER_LOOP()						\
  template <class ObjT, class RetT, C_LIST int TypeC, class FptrT>	\
  void ParMemberLoop ( ObjT *obj, RetT *ret,				\
		       FptrT fptr,					\
		       A_LIST						\
		       int start, int stop, int step = 1 ) {		\
    ParMemberLoop <ObjT, RetT, T_LIST, TypeC, FptrT>				\
      (obj, ret, fptr, P_LIST, start, stop, step);			\
  }

#define PAR_VOID_MEMBER_LOOP()						\
  template <class ObjT, C_LIST int TypeC, class FptrT>			\
  void ParVoidMemberLoop ( ObjT *obj,					\
			   FptrT fptr,					\
			   A_LIST					\
			   int start, int stop, int step = 1 ) {	\
    ParMemberLoop <ObjT, void, T_LIST, TypeC, FptrT>				\
      (obj, NULL, fptr, P_LIST, start, stop, step);			\
  }

#define ALL_LOOPS()				\
  PAR_VOID_LOOP()				\
  PAR_LOOP()					\
  PAR_VOID_MEMBER_LOOP()			\
  PAR_MEMBER_LOOP()				

#define T_LIST Arg1T, Arg2T, Arg3T, Arg4T, Arg5T
#define C_LIST class Arg1T, class Arg2T, class Arg3T, class Arg4T, class Arg5T,
#define A_LIST IN(Arg1T) arg1, IN(Arg2T) arg2, IN(Arg3T) arg3, IN(Arg4T) arg4, IN(Arg5T) arg5,
#define P_LIST arg1, arg2, arg3, arg4, arg5

PAR_VOID_LOOP();
PAR_LOOP();
PAR_VOID_MEMBER_LOOP();

#undef T_LIST
#undef C_LIST
#undef A_LIST
#undef P_LIST
#define T_LIST Arg1T, Arg2T, Arg3T, Arg4T, void
#define C_LIST class Arg1T, class Arg2T, class Arg3T, class Arg4T,
#define A_LIST IN(Arg1T) arg1, IN(Arg2T) arg2, IN(Arg3T) arg3, IN(Arg4T) arg4,
#define P_LIST arg1, arg2, arg3, arg4, NULL

ALL_LOOPS();

#undef T_LIST
#undef C_LIST
#undef A_LIST
#undef P_LIST
#define T_LIST Arg1T, Arg2T, Arg3T, void, void
#define C_LIST class Arg1T, class Arg2T, class Arg3T,
#define A_LIST IN(Arg1T) arg1, IN(Arg2T) arg2, IN(Arg3T) arg3,
#define P_LIST arg1, arg2, arg3, NULL, NULL

ALL_LOOPS();

#undef T_LIST
#undef C_LIST
#undef A_LIST
#undef P_LIST
#define T_LIST Arg1T, Arg2T, void, void, void
#define C_LIST class Arg1T, class Arg2T,
#define A_LIST IN(Arg1T) arg1, IN(Arg2T) arg2,
#define P_LIST arg1, arg2, NULL, NULL, NULL

ALL_LOOPS();

#undef T_LIST
#undef C_LIST
#undef A_LIST
#undef P_LIST
#define T_LIST Arg1T, void, void, void, void
#define C_LIST class Arg1T,
#define A_LIST IN(Arg1T) arg1,
#define P_LIST arg1, NULL, NULL, NULL, NULL

ALL_LOOPS();

#undef T_LIST
#undef C_LIST
#undef A_LIST
#undef P_LIST
#define T_LIST void, void, void, void, void
#define C_LIST 
#define A_LIST 
#define P_LIST NULL, NULL, NULL, NULL, NULL

ALL_LOOPS();

#undef T_LIST
#undef C_LIST
#undef A_LIST
#undef P_LIST
#undef PAR_VOID_LOOP
#undef PAR_LOOP
#undef PAR_VOID_MEMBER_LOOP
#undef PAR_MEMBER_LOOP
#undef ALL_LOOPS
#undef ITER
#undef SCALE_TD_POW2
#undef IN

#endif /* FUTURE_TEMP_HPP */
