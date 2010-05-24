#ifndef QTHREAD_SYNCVAR_HPP
#define QTHREAD_SYNCVAR_HPP

#include <qthread/qthread.h>

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

class syncvar
{
    public:
	virtual int empty();
	virtual int fill();
	virtual int readFF(uint64_t *const dest);
	virtual int readFE(uint64_t *const dest);
	virtual int writeF(const uint64_t * const src);
	virtual int writeEF(const uint64_t * const src);
	virtual uint64_t read() const;
	virtual void write(uint64_t src);
	virtual int status();
};

class strict_syncvar;

class loose_syncvar : public syncvar
{
    friend class onlysyncvar;
    /* constructors */
    QINLINE loose_syncvar(void) {
	the_syncvar_t.u.w = 0;
    }
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
    QINLINE const uint64_t operator op (const uint64_t &val) const { \
	uint64_t tmp = uint64_t(*this); \
	return tmp op##= val; \
    }
    _QT_ALL_OPS_(OVERLOAD_BINARY_ARITHMETIC_OPERATOR)
#undef OVERLOAD_BINARY_ARITHMETIC_OPERATOR

    /* comparison */
    QINLINE bool operator == (const uint64_t &val) const {
	return the_syncvar_t.u.s.data == val;
    }
    QINLINE bool operator != (const uint64_t &val) const {
	return the_syncvar_t.u.s.data != val;
    }

    /* cast operators */
    operator uint64_t() const
    {
	return the_syncvar_t.u.s.data;
    }
    operator syncvar_t()
    {
	return the_syncvar_t;
    }

    private:
	syncvar_t the_syncvar_t;
};

class strict_syncvar
{
    friend class loose_syncvar;
    /* constructors */
    QINLINE strict_syncvar(void) {
	the_syncvar_t.u.w = 0;
    }
    QINLINE strict_syncvar(const uint64_t &val) {
	the_syncvar_t.u.w = val;
    }
    QINLINE strict_syncvar(const syncvar_t &val) {
	the_syncvar_t.u.w = val.u.w;
    }

    /************************/
    /* overloaded operators */
    /************************/

    /* assignment operator */
    QINLINE strict_syncvar& operator =(const uint64_t &val) {
	qthread_syncvar_writeF(NULL, &the_syncvar_t, &val);
	return *this;
    }
    /* compound assignment operators */
#define OVERLOAD_ASSIGNMENT_OPERATOR(op) \
    QINLINE strict_syncvar& operator op##= (const uint64_t &val) { \
	uint64_t myval; \
	qthread_syncvar_readFE(NULL, &myval, &the_syncvar_t); \
	myval op##= val; \
	qthread_syncvar_writeEF(NULL, &the_syncvar_t, &myval); \
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
    QINLINE const uint64_t operator op (const uint64_t &val) { \
	uint64_t myval; \
	qthread_syncvar_readFF(NULL, &myval, &the_syncvar_t); \
	return myval op##= val; \
    }
    _QT_ALL_OPS_(OVERLOAD_BINARY_ARITHMETIC_OPERATOR)
#undef OVERLOAD_BINARY_ARITHMETIC_OPERATOR

    /* comparison */
    QINLINE bool operator == (const uint64_t &val) {
	uint64_t myval;
	qthread_syncvar_readFF(NULL, &myval, &the_syncvar_t);
	return myval == val;
    }
    QINLINE bool operator != (const uint64_t &val) {
	return !(*this == val);
    }

    /* cast operators */
    operator uint64_t()
    {
	uint64_t ret = 0;
	qthread_syncvar_readFF(NULL, &ret, &the_syncvar_t);
	return ret;
    }

    private:
	syncvar_t the_syncvar_t;
};

#undef _QT_ALL_OPS_

#endif
