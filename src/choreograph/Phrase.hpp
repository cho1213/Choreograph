/*
 * Copyright (c) 2014 David Wicks, sansumbrella.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

// Includes needed for tests to build (since they don't include the pch).
#include <functional>
#include <vector>

namespace choreograph
{
// A motion describes a one-dimensional change through time.
// These can be ease equations, always return the same value, or sample some data. Whatever you want.
// For classic motion, EaseFn( 0 ) = 0, EaseFn( 1 ) = 1.
// set( 0.0f ).move( )
// then again, might just make a curve struct that is callable and have a way to set its initial and final derivatives.
typedef std::function<float (float)> EaseFn;


/**
 \file
 Phrases for use in Choreograph.
 A Phrase is a part of a Sequence.
 It describes the motion between two positions.
 This is the essence of a Tween, with all values held internally.
 */

namespace
{
  /// Default ease function for holds. Keeps constant value output.
  inline float easeHold( float t ) { return 0.0f; }
  /// Default ease function for ramps.
  inline float easeNone( float t ) { return t; }

} // namespace

/// The default templated lerp function.
template<typename T>
T lerpT( const T &a, const T &b, float t )
{
  return a + (b - a) * t;
}

/**
 RampTo is a phrase that interpolates all components with an ease function.
 */
template<typename T>
class RampTo : public Source<T>
{
public:
  using LerpFn = std::function<T (const T&, const T&, float)>;

  RampTo( float start_time, float end_time, const T &start_value, const T &end_value, const EaseFn &ease_fn = &easeNone, const LerpFn &lerp_fn = &lerpT<T> ):
    Source<T>( start_time, end_time ),
    _start_value( start_value ),
    _end_value( end_value ),
    _easeFn( ease_fn ),
    _lerpFn( lerp_fn )
  {}

  /// Returns the interpolated value at the given time.
  T getValue( float atTime ) const override
  {
    return _lerpFn( _start_value, _end_value, _easeFn( this->normalizeTime( atTime ) ) );
  }

  T getStartValue() const override { return _start_value; }

  T getEndValue() const override { return _end_value; }

  SourceUniqueRef<T> clone() const override { return SourceUniqueRef<T>( new RampTo<T>( *this ) ); }

private:
  T       _start_value;
  T       _end_value;
  EaseFn  _easeFn;
  LerpFn  _lerpFn;
};


/**
 RampTo2 is a phrase with two separately-interpolated components.
 Allows for the use of separate ease functions per component.
 All components must be of the same type.
 */
template<typename T>
class RampTo2 : public Source<T>
{

public:

  RampTo2( float start_time, float end_time, const T &start_value, const T &end_value, const EaseFn &ease_x, const EaseFn &ease_y ):
    Source<T>( start_time, end_time ),
    _start_value( start_value ),
    _end_value( end_value ),
    _ease_x( ease_x ),
    _ease_y( ease_y )
  {}

  /// Returns the interpolated value at the given time.
  T getValue( float atTime ) const override
  {
    float t = this->normalizeTime( atTime );
    return T( componentLerpFn( getStartValue().x, getEndValue().x, _ease_x( t ) ),
              componentLerpFn( getStartValue().y, getEndValue().y, _ease_y( t ) ) );
  }

  T getStartValue() const override { return _start_value; }
  T getEndValue() const override { return _end_value; }

  SourceUniqueRef<T> clone() const override { return SourceUniqueRef<T>( new RampTo2<T>( *this ) ); }

private:
  using ComponentT = decltype( T().x ); // get the type of the x component;
  using ComponentLerpFn = std::function<ComponentT (const ComponentT&, const ComponentT&, float)>;

  ComponentLerpFn componentLerpFn = &lerpT<ComponentT>;
  T               _start_value;
  T               _end_value;
  EaseFn          _ease_x;
  EaseFn          _ease_y;
};

/**
 Hold is a phrase that hangs in there, never changing.
 */
template<typename T>
class Hold : public Source<T>
{
public:

  Hold( float start_time, float end_time, const T &start_value, const T &end_value ):
  Source<T>( start_time, end_time ),
  _value( end_value )
  {}

  T getValue( float atTime ) const override
  {
    return _value;
  }

  T getStartValue() const override
  {
    return _value;
  }

  T getEndValue() const override
  {
    return _value;
  }

  SourceUniqueRef<T> clone() const override { return SourceUniqueRef<T>( new Hold<T>( *this ) ); }

private:
  T       _value;
};

/**
 AnalyticChange is a phrase that calls a std::function every step.
 You could do similar things with a Motion's updateFn, but this is composable within Sequences.
 */
template<typename T>
class AnalyticChange : public Source<T>
{
public:
  /// Analytic Function receives start, end, and normalized time.
  /// Most basic would be mix( a, b, t ) or lerp( a, b, t ).
  /// Intended use is to apply something like cos() or random jitter.
  using Function = std::function<T (const T& startValue, const T& endValue, float normalizedTime, float duration)>;

  AnalyticChange( float start_time, float end_time, const T &start_value, const T &end_value, const Function &fn ):
    Source<T>( start_time, end_time ),
    _function( fn ),
    _start_value( start_value ),
    _end_value( end_value )
  {}

  T getValue( float atTime ) const override
  {
    return _function( _start_value, _end_value, this->normalizeTime( atTime ), this->getDuration() );
  }

  T getStartValue() const override { return _start_value; }
  T getEndValue() const override { return _end_value; }

  SourceUniqueRef<T> clone() const override { return SourceUniqueRef<T>( new AnalyticChange<T>( *this ) ); }

private:
  Function  _function;
  T         _start_value;
  T         _end_value;
};

} // namespace atlantic
