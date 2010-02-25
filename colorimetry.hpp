#ifndef COLORIMETRY_HPP
#define COLORIMETRY_HPP

#include "displayop.hpp"

static double smpte170m_green[ 3 ] = { 1.164383561643836, -0.391260370716072, -0.813004933873461 };
static double smpte170m_blue[ 3 ] = { 1.164383561643836,  2.017414758970775,  0.001127259960693 };
static double smpte170m_red[ 3 ] = { 1.164383561643836, -0.001054999706803,  1.595670195813386 };

static double itu709_green[ 3 ] = { 1.164165557121523,  -0.213138349939461,  -0.532748200973066 };
static double itu709_blue[ 3 ] = { 1.166544220758321,   2.112430116393991,   0.001144179685436 };
static double itu709_red[ 3 ] = { 1.164384394176109,   0.000813948963217,   1.793155612333230 };

static LoadMatrixCoefficients smpte170m( smpte170m_green, smpte170m_blue, smpte170m_red );
static LoadMatrixCoefficients itu709( itu709_green, itu709_blue, itu709_red );

#endif
