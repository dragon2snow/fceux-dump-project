#ifndef _OLDMOVIE_H_
#define _OLDMOVIE_H_

#include "movie.h"

enum EFCM_CONVERTRESULT
{
	FCM_CONVERTRESULT_SUCCESS,
	FCM_CONVERTRESULT_FAILOPEN,
	FCM_CONVERTRESULT_OLDVERSION,
	FCM_CONVERTRESULT_UNSUPPORTEDVERSION,
};

EFCM_CONVERTRESULT convert_fcm(MovieData& md, std::string fname);

#endif