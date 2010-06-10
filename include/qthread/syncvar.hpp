#ifndef QTHREAD_SYNCVAR_HPP
#define QTHREAD_SYNCVAR_HPP

#include <assert.h>
#include <qthread/qthread.h>

class syncvar
{
    public:
	QINLINE syncvar(void) { the_syncvar_t.u.w = 0; }
	virtual ~syncvar(void) {;}

	int empty(qthread_t *me = NULL) { return qthread_syncvar_empty(me, &the_syncvar_t); }
	int fill(qthread_t *me = NULL) { return qthread_syncvar_fill(me, &the_syncvar_t); }
	int readFF(qthread_t *me, uint64_t *const dest) { return qthread_syncvar_readFF(me, dest, &the_syncvar_t); }
	int readFF(uint64_t *const dest) { return qthread_syncvar_readFF(NULL, dest, &the_syncvar_t); }
	int readFE(qthread_t *me, uint64_t *const dest) { return qthread_syncvar_readFE(me, dest, &the_syncvar_t); }
	int readFE(uint64_t *const dest) { return qthread_syncvar_readFE(NULL, dest, &the_syncvar_t); }
	int writeF(qthread_t *me, const uint64_t * const src) { return qthread_syncvar_writeF(me, &the_syncvar_t, src); }
	int writeF(const uint64_t * const src) { return qthread_syncvar_writeF(NULL, &the_syncvar_t, src); }
	int writeEF(qthread_t *me, const uint64_t * const src) { return qthread_syncvar_writeEF(me, &the_syncvar_t, src); }
	int writeEF(const uint64_t * const src) { return qthread_syncvar_writeEF(NULL, &the_syncvar_t, src); }

	int status() { return qthread_syncvar_status(&the_syncvar_t); }
	uint64_t read() const { return the_syncvar_t.u.s.data; }
	void write(uint64_t src) {
	    assert((src>>60) == 0);
	    the_syncvar_t.u.s.data = src;
	}
    protected:
	uint64_t readFF(void) {
	    uint64_t ret = 0;
	    qthread_syncvar_readFF(NULL, &ret, &the_syncvar_t);
	    return ret;
	}
	syncvar_t the_syncvar_t;
};

#define _QT_ALL_OPS_(macro) \
    macro(+) \
    macro(-) \
    macro(*) \
    macro(/) \
    macro(%) \
    macro(^) \
    macro(|) \
    macro(&) \
    macro(<<) \
    macro(>>)

class loose_syncvar : public syncvar
{
    /* constructors */
    QINLINE loose_syncvar(const loose_syncvar &val) {
	the_syncvar_t.u.w = val.the_syncvar_t.u.w;
    }

    /************************/
    /* overloaded operators */
    /************************/

    /* assignment operator */
    QINLINE loose_syncvar& operator =(const loose_syncvar &val) {
	if (this != &val) {
	    the_syncvar_t.u.w = val.the_syncvar_t.u.w;
	    the_syncvar_t.u.s.lock = 0;
	}
	return *this;
    }
    QINLINE loose_syncvar& operator =(const uint64_t &val) {
	the_syncvar_t.u.s.data = val;
	return *this;
    }
    /* compound assignment operators */
#define OVERLOAD_ASSIGNMENT_OPERATOR(op) \
    QINLINE loose_syncvar& operator op##= (const uint64_t &val) { \
	the_syncvar_t.u.s.data op##= val; \
	return *this; \
    }
    _QT_ALL_OPS_(OVERLOAD_ASSIGNMENT_OPERATOR)
#undef OVERLOAD_ASSIGNMENT_OPERATOR
    QINLINE loose_syncvar& operator ++() { // prefix (++x)
	return *this += 1;
    }
    QINLINE loose_syncvar& operator --() { // prefix (--x)
	return *this -= 1;
    }
    QINLINE loose_syncvar operator ++(int) { // postfix (x++)
	loose_syncvar before = *this;
	this->operator++();
	return before;
    }
    QINLINE loose_syncvar operator --(int) { // postfix (x--)
	loose_syncvar before = *this;
	this->operator--();
	return before;
    }

    /* binary arithmetic */
#define OVERLOAD_BINARY_ARITHMETIC_OPERATOR(op) \
    QINLINE uint64_t operator op (const uint64_t &val) const { \
	uint64_t tmp = read(); \
	return tmp op##= val; \
    }
    _QT_ALL_OPS_(OVERLOAD_BINARY_ARITHMETIC_OPERATOR)
#undef OVERLOAD_BINARY_ARITHMETIC_OPERATOR

    /* cast operators */
    operator uint64_t() const { return read(); }
};

class strict_syncvar : public syncvar
{
    /* constructors */
    QINLINE strict_syncvar(const uint64_t &val) {
	the_syncvar_t.u.w = 0;
	the_syncvar_t.u.s.data = val;
    }
    QINLINE strict_syncvar(const syncvar_t &val) {
	the_syncvar_t.u.w = val.u.w;
    }

    /************************/
    /* overloaded operators */
    /************************/

    /* assignment operator */
    QINLINE strict_syncvar& operator =(const uint64_t &val) {
	writeF(&val);
	return *this;
    }
    /* compound assignment operators */
#define OVERLOAD_ASSIGNMENT_OPERATOR(op) \
    QINLINE strict_syncvar& operator op##= (const uint64_t &val) { \
	uint64_t myval; \
	qthread_t *me = qthread_self(); \
	readFE(me, &myval); \
	myval op##= val; \
	writeEF(me, &myval); \
	return *this; \
    }
    _QT_ALL_OPS_(OVERLOAD_ASSIGNMENT_OPERATOR)
#undef OVERLOAD_ASSIGNMENT_OPERATOR
    QINLINE strict_syncvar& operator ++() { // prefix (++x)
	return *this += 1;
    }
    QINLINE strict_syncvar& operator --() { // prefix (--x)
	return *this -= 1;
    }
    QINLINE strict_syncvar operator ++(int) { // postfix (x++)
	strict_syncvar before = *this;
	this->operator++();
	return before;
    }
    QINLINE strict_syncvar operator --(int) { // postfix (x--)
	strict_syncvar before = *this;
	this->operator--();
	return before;
    }

    /* binary arithmetic */
#define OVERLOAD_BINARY_ARITHMETIC_OPERATOR(op) \
    QINLINE uint64_t operator op (const uint64_t &val) { \
	uint64_t myval; \
	readFF(&myval); \
	return myval op##= val; \
    }
    _QT_ALL_OPS_(OVERLOAD_BINARY_ARITHMETIC_OPERATOR)
#undef OVERLOAD_BINARY_ARITHMETIC_OPERATOR

    /* cast operators */
    operator uint64_t() { return readFF(); }
};

#undef _QT_ALL_OPS_

#endif
