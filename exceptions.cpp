#include <stdio.h>

#include "exceptions.hpp"

void failure( const char *assertion, const char *file, int line, const char *function, AhabException *e )
{
  fprintf( stderr, "\nAssertion \"%s\" failed in file %s, function %s(), line #%d\n",
	   assertion, file, function, line );

  throw e;
}

void warn( const char *assertion, const char *file, int line, const char *function )
{
  fprintf( stderr, "\nAssertion \"%s\" failed in file %s, function %s(), line #%d\n",
	   assertion, file, function, line );
}

void UnixAssertError::print( void )
{
  fprintf( stderr, "Statement \"%s\" failed in file %s, function %s(), line #%d\n",
	   expr, file, function, line );
  fprintf( stderr, "Return value was %d, errno = %d (%s).\n",
	   result, errnumber, strerror( errnumber ) );
}
