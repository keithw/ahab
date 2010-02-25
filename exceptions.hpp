#ifndef EXCEPTIONS_HPP
#define EXCEPTIONS_HPP

#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

class AhabException { public: virtual ~AhabException() {} };
class SequenceNotFound : public AhabException {};
class ConformanceLimitExceeded : public AhabException {};
class UnixError : public AhabException
{
public:
  int err;
  UnixError( int s_errno ) { err = s_errno; }
};
class NotMPEGES : public AhabException {};
class InternalError : public AhabException {};
class MPEGInvalid : public AhabException {};
class NeedBits : public AhabException {};
class OutOfFrames : public AhabException {};
class DisplayError : public AhabException {};
class UnixAssertError : public AhabException
{
private:
  char *expr, *file, *function;
  int line, result, errnumber;
  
public:
  UnixAssertError( const char *s_expr, const char *s_file, int s_line, const char *s_function,
		   int s_result, int s_errnumber )
  {
    expr = strdup( s_expr );
    file = strdup( s_file );
    line = s_line;
    function = strdup( s_function );
    result = s_result;
    errnumber = s_errnumber;
  }

  void print( void );
};

void failure( const char *assertion, const char *file, int line, const char *function, AhabException *e );
void warn( const char *assertion, const char *file, int line, const char *function );

#define checkbound(ptr,check) \
do { \
if ( ptr > check ) { \
  fprintf( stderr, "%s:%d -- Pointer exceeds bound by %d bytes.\n", __FILE__, __LINE__, ptr - check ); \
  } \
} while( 0 ) \

#define unixassert(expr) do {\
   int assertion_result = (expr);\
   if ( assertion_result != 0 ) {\
     throw new UnixAssertError( __STRING(expr), __FILE__, __LINE__, __PRETTY_FUNCTION__, assertion_result, errno );\
   } } while ( 0 )

#define ahabassert(expr)                                                 \
  ((expr)                                                                \
   ? (void)0							         \
   : failure (__STRING(expr), __FILE__, __LINE__, __PRETTY_FUNCTION__, new InternalError() ))

#define ahabcomplain(expr)                                               \
  ((expr)                                                                \
   ? (void)0							         \
   : warn (__STRING(expr), __FILE__, __LINE__, __PRETTY_FUNCTION__ ))

#define mpegassert(expr)                                                 \
  ((expr)                                                                \
   ? (void)0							         \
   : failure (__STRING(expr), __FILE__, __LINE__, __PRETTY_FUNCTION__, new MPEGInvalid() ))

#endif
