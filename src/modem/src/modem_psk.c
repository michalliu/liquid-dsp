/*
 * Copyright (c) 2007 - 2014 Joseph Gaeddert
 *
 * This file is part of liquid.
 *
 * liquid is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * liquid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with liquid.  If not, see <http://www.gnu.org/licenses/>.
 */

//
// modem_psk.c
//

// create a psk (phase-shift keying) modem object
MODEM() MODEM(_create_psk)(unsigned int _bits_per_symbol)
{
    MODEM() q = (MODEM()) malloc( sizeof(struct MODEM(_s)) );

    switch (_bits_per_symbol) {
    case 1: q->scheme = LIQUID_MODEM_PSK2;   break;
    case 2: q->scheme = LIQUID_MODEM_PSK4;   break;
    case 3: q->scheme = LIQUID_MODEM_PSK8;   break;
    case 4: q->scheme = LIQUID_MODEM_PSK16;  break;
    case 5: q->scheme = LIQUID_MODEM_PSK32;  break;
    case 6: q->scheme = LIQUID_MODEM_PSK64;  break;
    case 7: q->scheme = LIQUID_MODEM_PSK128; break;
    case 8: q->scheme = LIQUID_MODEM_PSK256; break;
    default:
        fprintf(stderr,"error: modem_create_psk(), cannot support PSK with m > 8\n");
        exit(1);
    }

    // initialize basic modem structure
    MODEM(_init)(q, _bits_per_symbol);

    // compute alpha
    q->data.psk.alpha = M_PI/(T)(q->M);

    // initialize demodulation array reference
    unsigned int k;
    for (k=0; k<(q->m); k++)
#if LIQUID_FPM
        q->ref[k] = Q(_angle_float_to_fixed)((1<<k) * q->data.psk.alpha);
#else
        q->ref[k] = (1<<k) * q->data.psk.alpha;
#endif

    // compute phase offset (half of phase difference between symbols)
#if LIQUID_FPM
    q->data.psk.d_phi = Q(_pi) - Q(_pi)/q->M;
#else
    q->data.psk.d_phi = M_PI*(1.0f - 1.0f/(T)(q->M));
#endif

    // set modulation/demodulation functions
    q->modulate_func = &MODEM(_modulate_psk);
    q->demodulate_func = &MODEM(_demodulate_psk);

    // initialize symbol map
    q->symbol_map = (TC*)malloc(q->M*sizeof(TC));
    MODEM(_init_map)(q);
    q->modulate_using_map = 1;

    // initialize soft-demodulation look-up table
    if (q->m >= 3)
        MODEM(_demodsoft_gentab)(q, 2);

    // reset and return
    MODEM(_reset)(q);
    return q;
}

// modulate PSK
void MODEM(_modulate_psk)(MODEM()      _q,
                          unsigned int _sym_in,
                          TC *         _y)
{
    // 'encode' input symbol (actually gray decoding)
    _sym_in = gray_decode(_sym_in);

    // compute output sample
    float complex v = liquid_cexpjf(_sym_in * 2 * _q->data.psk.alpha );
#if LIQUID_FPM
    *_y = CQ(_float_to_fixed)(v);
#else
    *_y = v;
#endif
}

// demodulate PSK
void MODEM(_demodulate_psk)(MODEM()        _q,
                            TC             _x,
                            unsigned int * _sym_out)
{
    // compute angle and subtract phase offset, ensuring phase is in [-pi,pi)
#if LIQUID_FPM
    T theta = CQ(_carg)(_x) - _q->data.psk.d_phi;
    if (theta < -Q(_pi))
        theta += Q(_2pi);
#else
    T theta = cargf(_x) - _q->data.psk.d_phi;
    if (theta < -M_PI)
        theta += 2*M_PI;
#endif

    // demodulate on linearly-spaced array
    unsigned int s;             // demodulated symbol
    T demod_phase_error;        // demodulation phase error
    MODEM(_demodulate_linear_array_ref)(theta, _q->m, _q->ref, &s, &demod_phase_error);

    // 'decode' output symbol (actually gray encoding)
    *_sym_out = gray_encode(s);

    // re-modulate symbol and store state
    MODEM(_modulate)(_q, *_sym_out, &_q->x_hat);
    _q->r = _x;
}

