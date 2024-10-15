/*
 * AAC Spectral Band Replication decoding functions
 * Copyright (c) 2008-2009 Robert Swain ( rob opendot cl )
 * Copyright (c) 2009-2010 Alex Converse <alex.converse@gmail.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "config.h"
#include "libavutil/attributes_internal.h"
#include "libavutil/mem_internal.h"

static void sbr_sum64x5_c(INTFLOAT *z)
{
    int k;
    for (k = 0; k < 64; k++) {
        INTFLOAT f = z[k] + z[k + 64] + z[k + 128] + z[k + 192] + z[k + 256];
        z[k] = f;
    }
}

static void sbr_qmf_deint_bfly_c(INTFLOAT *v, const INTFLOAT *src0, const INTFLOAT *src1)
{
    int i;
    for (i = 0; i < 64; i++) {
#if USE_FIXED
        v[      i] = (int)(0x10U + src0[i] - src1[63 - i]) >> 5;
        v[127 - i] = (int)(0x10U + src0[i] + src1[63 - i]) >> 5;
#else
        v[      i] = src0[i] - src1[63 - i];
        v[127 - i] = src0[i] + src1[63 - i];
#endif
    }
}

static void sbr_hf_apply_noise_0(INTFLOAT (*Y)[2], const AAC_FLOAT *s_m,
                                 const AAC_FLOAT *q_filt, int noise,
                                 int kx, int m_max)
{
    sbr_hf_apply_noise(Y, s_m, q_filt, noise, (INTFLOAT)1.0, (INTFLOAT)0.0, m_max);
}

static void sbr_hf_apply_noise_1(INTFLOAT (*Y)[2], const AAC_FLOAT *s_m,
                                 const AAC_FLOAT *q_filt, int noise,
                                 int kx, int m_max)
{
    INTFLOAT phi_sign = 1 - 2 * (kx & 1);
    sbr_hf_apply_noise(Y, s_m, q_filt, noise, (INTFLOAT)0.0, phi_sign, m_max);
}

static void sbr_hf_apply_noise_2(INTFLOAT (*Y)[2], const AAC_FLOAT *s_m,
                                 const AAC_FLOAT *q_filt, int noise,
                                 int kx, int m_max)
{
    sbr_hf_apply_noise(Y, s_m, q_filt, noise, (INTFLOAT)-1.0, (INTFLOAT)0.0, m_max);
}

static void sbr_hf_apply_noise_3(INTFLOAT (*Y)[2], const AAC_FLOAT *s_m,
                                 const AAC_FLOAT *q_filt, int noise,
                                 int kx, int m_max)
{
    INTFLOAT phi_sign = 1 - 2 * (kx & 1);
    sbr_hf_apply_noise(Y, s_m, q_filt, noise, (INTFLOAT)0.0, -phi_sign, m_max);
}

av_cold void AAC_RENAME(ff_sbrdsp_init)(SBRDSPContext *s)
{
    s->sum64x5 = sbr_sum64x5_c;
    s->sum_square = sbr_sum_square_c;
    s->neg_odd_64 = sbr_neg_odd_64_c;
    s->qmf_pre_shuffle = sbr_qmf_pre_shuffle_c;
    s->qmf_post_shuffle = sbr_qmf_post_shuffle_c;
    s->qmf_deint_neg = sbr_qmf_deint_neg_c;
    s->qmf_deint_bfly = sbr_qmf_deint_bfly_c;
    s->autocorrelate = sbr_autocorrelate_c;
    s->hf_gen = sbr_hf_gen_c;
    s->hf_g_filt = sbr_hf_g_filt_c;

    s->hf_apply_noise[0] = sbr_hf_apply_noise_0;
    s->hf_apply_noise[1] = sbr_hf_apply_noise_1;
    s->hf_apply_noise[2] = sbr_hf_apply_noise_2;
    s->hf_apply_noise[3] = sbr_hf_apply_noise_3;

#if !USE_FIXED
#if ARCH_ARM
    ff_sbrdsp_init_arm(s);
#elif ARCH_AARCH64
    ff_sbrdsp_init_aarch64(s);
#elif ARCH_RISCV
    ff_sbrdsp_init_riscv(s);
#elif ARCH_X86
    ff_sbrdsp_init_x86(s);
#endif
#endif /* !USE_FIXED */
}

/* First eight entries repeated at end to simplify SIMD implementations. */
const attribute_visibility_hidden DECLARE_ALIGNED(16, INTFLOAT, AAC_RENAME(ff_sbr_noise_table))[][2] = {
{Q31(-0.99948153278296f), Q31(-0.59483417516607f)}, {Q31( 0.97113454393991f), Q31(-0.67528515225647f)},
{Q31( 0.14130051758487f), Q31(-0.95090983575689f)}, {Q31(-0.47005496701697f), Q31(-0.37340549728647f)},
{Q31( 0.80705063769351f), Q31( 0.29653668284408f)}, {Q31(-0.38981478896926f), Q31( 0.89572605717087f)},
{Q31(-0.01053049862020f), Q31(-0.66959058036166f)}, {Q31(-0.91266367957293f), Q31(-0.11522938140034f)},
{Q31( 0.54840422910309f), Q31( 0.75221367176302f)}, {Q31( 0.40009252867955f), Q31(-0.98929400334421f)},
{Q31(-0.99867974711855f), Q31(-0.88147068645358f)}, {Q31(-0.95531076805040f), Q31( 0.90908757154593f)},
{Q31(-0.45725933317144f), Q31(-0.56716323646760f)}, {Q31(-0.72929675029275f), Q31(-0.98008272727324f)},
{Q31( 0.75622801399036f), Q31( 0.20950329995549f)}, {Q31( 0.07069442601050f), Q31(-0.78247898470706f)},
{Q31( 0.74496252926055f), Q31(-0.91169004445807f)}, {Q31(-0.96440182703856f), Q31(-0.94739918296622f)},
{Q31( 0.30424629369539f), Q31(-0.49438267012479f)}, {Q31( 0.66565033746925f), Q31( 0.64652935542491f)},
{Q31( 0.91697008020594f), Q31( 0.17514097332009f)}, {Q31(-0.70774918760427f), Q31( 0.52548653416543f)},
{Q31(-0.70051415345560f), Q31(-0.45340028808763f)}, {Q31(-0.99496513054797f), Q31(-0.90071908066973f)},
{Q31( 0.98164490790123f), Q31(-0.77463155528697f)}, {Q31(-0.54671580548181f), Q31(-0.02570928536004f)},
{Q31(-0.01689629065389f), Q31( 0.00287506445732f)}, {Q31(-0.86110349531986f), Q31( 0.42548583726477f)},
{Q31(-0.98892980586032f), Q31(-0.87881132267556f)}, {Q31( 0.51756627678691f), Q31( 0.66926784710139f)},
{Q31(-0.99635026409640f), Q31(-0.58107730574765f)}, {Q31(-0.99969370862163f), Q31( 0.98369989360250f)},
{Q31( 0.55266258627194f), Q31( 0.59449057465591f)}, {Q31( 0.34581177741673f), Q31( 0.94879421061866f)},
{Q31( 0.62664209577999f), Q31(-0.74402970906471f)}, {Q31(-0.77149701404973f), Q31(-0.33883658042801f)},
{Q31(-0.91592244254432f), Q31( 0.03687901376713f)}, {Q31(-0.76285492357887f), Q31(-0.91371867919124f)},
{Q31( 0.79788337195331f), Q31(-0.93180971199849f)}, {Q31( 0.54473080610200f), Q31(-0.11919206037186f)},
{Q31(-0.85639281671058f), Q31( 0.42429854760451f)}, {Q31(-0.92882402971423f), Q31( 0.27871809078609f)},
{Q31(-0.11708371046774f), Q31(-0.99800843444966f)}, {Q31( 0.21356749817493f), Q31(-0.90716295627033f)},
{Q31(-0.76191692573909f), Q31( 0.99768118356265f)}, {Q31( 0.98111043100884f), Q31(-0.95854459734407f)},
{Q31(-0.85913269895572f), Q31( 0.95766566168880f)}, {Q31(-0.93307242253692f), Q31( 0.49431757696466f)},
{Q31( 0.30485754879632f), Q31(-0.70540034357529f)}, {Q31( 0.85289650925190f), Q31( 0.46766131791044f)},
{Q31( 0.91328082618125f), Q31(-0.99839597361769f)}, {Q31(-0.05890199924154f), Q31( 0.70741827819497f)},
{Q31( 0.28398686150148f), Q31( 0.34633555702188f)}, {Q31( 0.95258164539612f), Q31(-0.54893416026939f)},
{Q31(-0.78566324168507f), Q31(-0.75568541079691f)}, {Q31(-0.95789495447877f), Q31(-0.20423194696966f)},
{Q31( 0.82411158711197f), Q31( 0.96654618432562f)}, {Q31(-0.65185446735885f), Q31(-0.88734990773289f)},
{Q31(-0.93643603134666f), Q31( 0.99870790442385f)}, {Q31( 0.91427159529618f), Q31(-0.98290505544444f)},
{Q31(-0.70395684036886f), Q31( 0.58796798221039f)}, {Q31( 0.00563771969365f), Q31( 0.61768196727244f)},
{Q31( 0.89065051931895f), Q31( 0.52783352697585f)}, {Q31(-0.68683707712762f), Q31( 0.80806944710339f)},
{Q31( 0.72165342518718f), Q31(-0.69259857349564f)}, {Q31(-0.62928247730667f), Q31( 0.13627037407335f)},
{Q31( 0.29938434065514f), Q31(-0.46051329682246f)}, {Q31(-0.91781958879280f), Q31(-0.74012716684186f)},
{Q31( 0.99298717043688f), Q31( 0.40816610075661f)}, {Q31( 0.82368298622748f), Q31(-0.74036047190173f)},
{Q31(-0.98512833386833f), Q31(-0.99972330709594f)}, {Q31(-0.95915368242257f), Q31(-0.99237800466040f)},
{Q31(-0.21411126572790f), Q31(-0.93424819052545f)}, {Q31(-0.68821476106884f), Q31(-0.26892306315457f)},
{Q31( 0.91851997982317f), Q31( 0.09358228901785f)}, {Q31(-0.96062769559127f), Q31( 0.36099095133739f)},
{Q31( 0.51646184922287f), Q31(-0.71373332873917f)}, {Q31( 0.61130721139669f), Q31( 0.46950141175917f)},
{Q31( 0.47336129371299f), Q31(-0.27333178296162f)}, {Q31( 0.90998308703519f), Q31( 0.96715662938132f)},
{Q31( 0.44844799194357f), Q31( 0.99211574628306f)}, {Q31( 0.66614891079092f), Q31( 0.96590176169121f)},
{Q31( 0.74922239129237f), Q31(-0.89879858826087f)}, {Q31(-0.99571588506485f), Q31( 0.52785521494349f)},
{Q31( 0.97401082477563f), Q31(-0.16855870075190f)}, {Q31( 0.72683747733879f), Q31(-0.48060774432251f)},
{Q31( 0.95432193457128f), Q31( 0.68849603408441f)}, {Q31(-0.72962208425191f), Q31(-0.76608443420917f)},
{Q31(-0.85359479233537f), Q31( 0.88738125901579f)}, {Q31(-0.81412430338535f), Q31(-0.97480768049637f)},
{Q31(-0.87930772356786f), Q31( 0.74748307690436f)}, {Q31(-0.71573331064977f), Q31(-0.98570608178923f)},
{Q31( 0.83524300028228f), Q31( 0.83702537075163f)}, {Q31(-0.48086065601423f), Q31(-0.98848504923531f)},
{Q31( 0.97139128574778f), Q31( 0.80093621198236f)}, {Q31( 0.51992825347895f), Q31( 0.80247631400510f)},
{Q31(-0.00848591195325f), Q31(-0.76670128000486f)}, {Q31(-0.70294374303036f), Q31( 0.55359910445577f)},
{Q31(-0.95894428168140f), Q31(-0.43265504344783f)}, {Q31( 0.97079252950321f), Q31( 0.09325857238682f)},
{Q31(-0.92404293670797f), Q31( 0.85507704027855f)}, {Q31(-0.69506469500450f), Q31( 0.98633412625459f)},
{Q31( 0.26559203620024f), Q31( 0.73314307966524f)}, {Q31( 0.28038443336943f), Q31( 0.14537913654427f)},
{Q31(-0.74138124825523f), Q31( 0.99310339807762f)}, {Q31(-0.01752795995444f), Q31(-0.82616635284178f)},
{Q31(-0.55126773094930f), Q31(-0.98898543862153f)}, {Q31( 0.97960898850996f), Q31(-0.94021446752851f)},
{Q31(-0.99196309146936f), Q31( 0.67019017358456f)}, {Q31(-0.67684928085260f), Q31( 0.12631491649378f)},
{Q31( 0.09140039465500f), Q31(-0.20537731453108f)}, {Q31(-0.71658965751996f), Q31(-0.97788200391224f)},
{Q31( 0.81014640078925f), Q31( 0.53722648362443f)}, {Q31( 0.40616991671205f), Q31(-0.26469008598449f)},
{Q31(-0.67680188682972f), Q31( 0.94502052337695f)}, {Q31( 0.86849774348749f), Q31(-0.18333598647899f)},
{Q31(-0.99500381284851f), Q31(-0.02634122068550f)}, {Q31( 0.84329189340667f), Q31( 0.10406957462213f)},
{Q31(-0.09215968531446f), Q31( 0.69540012101253f)}, {Q31( 0.99956173327206f), Q31(-0.12358542001404f)},
{Q31(-0.79732779473535f), Q31(-0.91582524736159f)}, {Q31( 0.96349973642406f), Q31( 0.96640458041000f)},
{Q31(-0.79942778496547f), Q31( 0.64323902822857f)}, {Q31(-0.11566039853896f), Q31( 0.28587846253726f)},
{Q31(-0.39922954514662f), Q31( 0.94129601616966f)}, {Q31( 0.99089197565987f), Q31(-0.92062625581587f)},
{Q31( 0.28631285179909f), Q31(-0.91035047143603f)}, {Q31(-0.83302725605608f), Q31(-0.67330410892084f)},
{Q31( 0.95404443402072f), Q31( 0.49162765398743f)}, {Q31(-0.06449863579434f), Q31( 0.03250560813135f)},
{Q31(-0.99575054486311f), Q31( 0.42389784469507f)}, {Q31(-0.65501142790847f), Q31( 0.82546114655624f)},
{Q31(-0.81254441908887f), Q31(-0.51627234660629f)}, {Q31(-0.99646369485481f), Q31( 0.84490533520752f)},
{Q31( 0.00287840603348f), Q31( 0.64768261158166f)}, {Q31( 0.70176989408455f), Q31(-0.20453028573322f)},
{Q31( 0.96361882270190f), Q31( 0.40706967140989f)}, {Q31(-0.68883758192426f), Q31( 0.91338958840772f)},
{Q31(-0.34875585502238f), Q31( 0.71472290693300f)}, {Q31( 0.91980081243087f), Q31( 0.66507455644919f)},
{Q31(-0.99009048343881f), Q31( 0.85868021604848f)}, {Q31( 0.68865791458395f), Q31( 0.55660316809678f)},
{Q31(-0.99484402129368f), Q31(-0.20052559254934f)}, {Q31( 0.94214511408023f), Q31(-0.99696425367461f)},
{Q31(-0.67414626793544f), Q31( 0.49548221180078f)}, {Q31(-0.47339353684664f), Q31(-0.85904328834047f)},
{Q31( 0.14323651387360f), Q31(-0.94145598222488f)}, {Q31(-0.29268293575672f), Q31( 0.05759224927952f)},
{Q31( 0.43793861458754f), Q31(-0.78904969892724f)}, {Q31(-0.36345126374441f), Q31( 0.64874435357162f)},
{Q31(-0.08750604656825f), Q31( 0.97686944362527f)}, {Q31(-0.96495267812511f), Q31(-0.53960305946511f)},
{Q31( 0.55526940659947f), Q31( 0.78891523734774f)}, {Q31( 0.73538215752630f), Q31( 0.96452072373404f)},
{Q31(-0.30889773919437f), Q31(-0.80664389776860f)}, {Q31( 0.03574995626194f), Q31(-0.97325616900959f)},
{Q31( 0.98720684660488f), Q31( 0.48409133691962f)}, {Q31(-0.81689296271203f), Q31(-0.90827703628298f)},
{Q31( 0.67866860118215f), Q31( 0.81284503870856f)}, {Q31(-0.15808569732583f), Q31( 0.85279555024382f)},
{Q31( 0.80723395114371f), Q31(-0.24717418514605f)}, {Q31( 0.47788757329038f), Q31(-0.46333147839295f)},
{Q31( 0.96367554763201f), Q31( 0.38486749303242f)}, {Q31(-0.99143875716818f), Q31(-0.24945277239809f)},
{Q31( 0.83081876925833f), Q31(-0.94780851414763f)}, {Q31(-0.58753191905341f), Q31( 0.01290772389163f)},
{Q31( 0.95538108220960f), Q31(-0.85557052096538f)}, {Q31(-0.96490920476211f), Q31(-0.64020970923102f)},
{Q31(-0.97327101028521f), Q31( 0.12378128133110f)}, {Q31( 0.91400366022124f), Q31( 0.57972471346930f)},
{Q31(-0.99925837363824f), Q31( 0.71084847864067f)}, {Q31(-0.86875903507313f), Q31(-0.20291699203564f)},
{Q31(-0.26240034795124f), Q31(-0.68264554369108f)}, {Q31(-0.24664412953388f), Q31(-0.87642273115183f)},
{Q31( 0.02416275806869f), Q31( 0.27192914288905f)}, {Q31( 0.82068619590515f), Q31(-0.85087787994476f)},
{Q31( 0.88547373760759f), Q31(-0.89636802901469f)}, {Q31(-0.18173078152226f), Q31(-0.26152145156800f)},
{Q31( 0.09355476558534f), Q31( 0.54845123045604f)}, {Q31(-0.54668414224090f), Q31( 0.95980774020221f)},
{Q31( 0.37050990604091f), Q31(-0.59910140383171f)}, {Q31(-0.70373594262891f), Q31( 0.91227665827081f)},
{Q31(-0.34600785879594f), Q31(-0.99441426144200f)}, {Q31(-0.68774481731008f), Q31(-0.30238837956299f)},
{Q31(-0.26843291251234f), Q31( 0.83115668004362f)}, {Q31( 0.49072334613242f), Q31(-0.45359708737775f)},
{Q31( 0.38975993093975f), Q31( 0.95515358099121f)}, {Q31(-0.97757125224150f), Q31( 0.05305894580606f)},
{Q31(-0.17325552859616f), Q31(-0.92770672250494f)}, {Q31( 0.99948035025744f), Q31( 0.58285545563426f)},
{Q31(-0.64946246527458f), Q31( 0.68645507104960f)}, {Q31(-0.12016920576437f), Q31(-0.57147322153312f)},
{Q31(-0.58947456517751f), Q31(-0.34847132454388f)}, {Q31(-0.41815140454465f), Q31( 0.16276422358861f)},
{Q31( 0.99885650204884f), Q31( 0.11136095490444f)}, {Q31(-0.56649614128386f), Q31(-0.90494866361587f)},
{Q31( 0.94138021032330f), Q31( 0.35281916733018f)}, {Q31(-0.75725076534641f), Q31( 0.53650549640587f)},
{Q31( 0.20541973692630f), Q31(-0.94435144369918f)}, {Q31( 0.99980371023351f), Q31( 0.79835913565599f)},
{Q31( 0.29078277605775f), Q31( 0.35393777921520f)}, {Q31(-0.62858772103030f), Q31( 0.38765693387102f)},
{Q31( 0.43440904467688f), Q31(-0.98546330463232f)}, {Q31(-0.98298583762390f), Q31( 0.21021524625209f)},
{Q31( 0.19513029146934f), Q31(-0.94239832251867f)}, {Q31(-0.95476662400101f), Q31( 0.98364554179143f)},
{Q31( 0.93379635304810f), Q31(-0.70881994583682f)}, {Q31(-0.85235410573336f), Q31(-0.08342347966410f)},
{Q31(-0.86425093011245f), Q31(-0.45795025029466f)}, {Q31( 0.38879779059045f), Q31( 0.97274429344593f)},
{Q31( 0.92045124735495f), Q31(-0.62433652524220f)}, {Q31( 0.89162532251878f), Q31( 0.54950955570563f)},
{Q31(-0.36834336949252f), Q31( 0.96458298020975f)}, {Q31( 0.93891760988045f), Q31(-0.89968353740388f)},
{Q31( 0.99267657565094f), Q31(-0.03757034316958f)}, {Q31(-0.94063471614176f), Q31( 0.41332338538963f)},
{Q31( 0.99740224117019f), Q31(-0.16830494996370f)}, {Q31(-0.35899413170555f), Q31(-0.46633226649613f)},
{Q31( 0.05237237274947f), Q31(-0.25640361602661f)}, {Q31( 0.36703583957424f), Q31(-0.38653265641875f)},
{Q31( 0.91653180367913f), Q31(-0.30587628726597f)}, {Q31( 0.69000803499316f), Q31( 0.90952171386132f)},
{Q31(-0.38658751133527f), Q31( 0.99501571208985f)}, {Q31(-0.29250814029851f), Q31( 0.37444994344615f)},
{Q31(-0.60182204677608f), Q31( 0.86779651036123f)}, {Q31(-0.97418588163217f), Q31( 0.96468523666475f)},
{Q31( 0.88461574003963f), Q31( 0.57508405276414f)}, {Q31( 0.05198933055162f), Q31( 0.21269661669964f)},
{Q31(-0.53499621979720f), Q31( 0.97241553731237f)}, {Q31(-0.49429560226497f), Q31( 0.98183865291903f)},
{Q31(-0.98935142339139f), Q31(-0.40249159006933f)}, {Q31(-0.98081380091130f), Q31(-0.72856895534041f)},
{Q31(-0.27338148835532f), Q31( 0.99950922447209f)}, {Q31( 0.06310802338302f), Q31(-0.54539587529618f)},
{Q31(-0.20461677199539f), Q31(-0.14209977628489f)}, {Q31( 0.66223843141647f), Q31( 0.72528579940326f)},
{Q31(-0.84764345483665f), Q31( 0.02372316801261f)}, {Q31(-0.89039863483811f), Q31( 0.88866581484602f)},
{Q31( 0.95903308477986f), Q31( 0.76744927173873f)}, {Q31( 0.73504123909879f), Q31(-0.03747203173192f)},
{Q31(-0.31744434966056f), Q31(-0.36834111883652f)}, {Q31(-0.34110827591623f), Q31( 0.40211222807691f)},
{Q31( 0.47803883714199f), Q31(-0.39423219786288f)}, {Q31( 0.98299195879514f), Q31( 0.01989791390047f)},
{Q31(-0.30963073129751f), Q31(-0.18076720599336f)}, {Q31( 0.99992588229018f), Q31(-0.26281872094289f)},
{Q31(-0.93149731080767f), Q31(-0.98313162570490f)}, {Q31( 0.99923472302773f), Q31(-0.80142993767554f)},
{Q31(-0.26024169633417f), Q31(-0.75999759855752f)}, {Q31(-0.35712514743563f), Q31( 0.19298963768574f)},
{Q31(-0.99899084509530f), Q31( 0.74645156992493f)}, {Q31( 0.86557171579452f), Q31( 0.55593866696299f)},
{Q31( 0.33408042438752f), Q31( 0.86185953874709f)}, {Q31( 0.99010736374716f), Q31( 0.04602397576623f)},
{Q31(-0.66694269691195f), Q31(-0.91643611810148f)}, {Q31( 0.64016792079480f), Q31( 0.15649530836856f)},
{Q31( 0.99570534804836f), Q31( 0.45844586038111f)}, {Q31(-0.63431466947340f), Q31( 0.21079116459234f)},
{Q31(-0.07706847005931f), Q31(-0.89581437101329f)}, {Q31( 0.98590090577724f), Q31( 0.88241721133981f)},
{Q31( 0.80099335254678f), Q31(-0.36851896710853f)}, {Q31( 0.78368131392666f), Q31( 0.45506999802597f)},
{Q31( 0.08707806671691f), Q31( 0.80938994918745f)}, {Q31(-0.86811883080712f), Q31( 0.39347308654705f)},
{Q31(-0.39466529740375f), Q31(-0.66809432114456f)}, {Q31( 0.97875325649683f), Q31(-0.72467840967746f)},
{Q31(-0.95038560288864f), Q31( 0.89563219587625f)}, {Q31( 0.17005239424212f), Q31( 0.54683053962658f)},
{Q31(-0.76910792026848f), Q31(-0.96226617549298f)}, {Q31( 0.99743281016846f), Q31( 0.42697157037567f)},
{Q31( 0.95437383549973f), Q31( 0.97002324109952f)}, {Q31( 0.99578905365569f), Q31(-0.54106826257356f)},
{Q31( 0.28058259829990f), Q31(-0.85361420634036f)}, {Q31( 0.85256524470573f), Q31(-0.64567607735589f)},
{Q31(-0.50608540105128f), Q31(-0.65846015480300f)}, {Q31(-0.97210735183243f), Q31(-0.23095213067791f)},
{Q31( 0.95424048234441f), Q31(-0.99240147091219f)}, {Q31(-0.96926570524023f), Q31( 0.73775654896574f)},
{Q31( 0.30872163214726f), Q31( 0.41514960556126f)}, {Q31(-0.24523839572639f), Q31( 0.63206633394807f)},
{Q31(-0.33813265086024f), Q31(-0.38661779441897f)}, {Q31(-0.05826828420146f), Q31(-0.06940774188029f)},
{Q31(-0.22898461455054f), Q31( 0.97054853316316f)}, {Q31(-0.18509915019881f), Q31( 0.47565762892084f)},
{Q31(-0.10488238045009f), Q31(-0.87769947402394f)}, {Q31(-0.71886586182037f), Q31( 0.78030982480538f)},
{Q31( 0.99793873738654f), Q31( 0.90041310491497f)}, {Q31( 0.57563307626120f), Q31(-0.91034337352097f)},
{Q31( 0.28909646383717f), Q31( 0.96307783970534f)}, {Q31( 0.42188998312520f), Q31( 0.48148651230437f)},
{Q31( 0.93335049681047f), Q31(-0.43537023883588f)}, {Q31(-0.97087374418267f), Q31( 0.86636445711364f)},
{Q31( 0.36722871286923f), Q31( 0.65291654172961f)}, {Q31(-0.81093025665696f), Q31( 0.08778370229363f)},
{Q31(-0.26240603062237f), Q31(-0.92774095379098f)}, {Q31( 0.83996497984604f), Q31( 0.55839849139647f)},
{Q31(-0.99909615720225f), Q31(-0.96024605713970f)}, {Q31( 0.74649464155061f), Q31( 0.12144893606462f)},
{Q31(-0.74774595569805f), Q31(-0.26898062008959f)}, {Q31( 0.95781667469567f), Q31(-0.79047927052628f)},
{Q31( 0.95472308713099f), Q31(-0.08588776019550f)}, {Q31( 0.48708332746299f), Q31( 0.99999041579432f)},
{Q31( 0.46332038247497f), Q31( 0.10964126185063f)}, {Q31(-0.76497004940162f), Q31( 0.89210929242238f)},
{Q31( 0.57397389364339f), Q31( 0.35289703373760f)}, {Q31( 0.75374316974495f), Q31( 0.96705214651335f)},
{Q31(-0.59174397685714f), Q31(-0.89405370422752f)}, {Q31( 0.75087906691890f), Q31(-0.29612672982396f)},
{Q31(-0.98607857336230f), Q31( 0.25034911730023f)}, {Q31(-0.40761056640505f), Q31(-0.90045573444695f)},
{Q31( 0.66929266740477f), Q31( 0.98629493401748f)}, {Q31(-0.97463695257310f), Q31(-0.00190223301301f)},
{Q31( 0.90145509409859f), Q31( 0.99781390365446f)}, {Q31(-0.87259289048043f), Q31( 0.99233587353666f)},
{Q31(-0.91529461447692f), Q31(-0.15698707534206f)}, {Q31(-0.03305738840705f), Q31(-0.37205262859764f)},
{Q31( 0.07223051368337f), Q31(-0.88805001733626f)}, {Q31( 0.99498012188353f), Q31( 0.97094358113387f)},
{Q31(-0.74904939500519f), Q31( 0.99985483641521f)}, {Q31( 0.04585228574211f), Q31( 0.99812337444082f)},
{Q31(-0.89054954257993f), Q31(-0.31791913188064f)}, {Q31(-0.83782144651251f), Q31( 0.97637632547466f)},
{Q31( 0.33454804933804f), Q31(-0.86231516800408f)}, {Q31(-0.99707579362824f), Q31( 0.93237990079441f)},
{Q31(-0.22827527843994f), Q31( 0.18874759397997f)}, {Q31( 0.67248046289143f), Q31(-0.03646211390569f)},
{Q31(-0.05146538187944f), Q31(-0.92599700120679f)}, {Q31( 0.99947295749905f), Q31( 0.93625229707912f)},
{Q31( 0.66951124390363f), Q31( 0.98905825623893f)}, {Q31(-0.99602956559179f), Q31(-0.44654715757688f)},
{Q31( 0.82104905483590f), Q31( 0.99540741724928f)}, {Q31( 0.99186510988782f), Q31( 0.72023001312947f)},
{Q31(-0.65284592392918f), Q31( 0.52186723253637f)}, {Q31( 0.93885443798188f), Q31(-0.74895312615259f)},
{Q31( 0.96735248738388f), Q31( 0.90891816978629f)}, {Q31(-0.22225968841114f), Q31( 0.57124029781228f)},
{Q31(-0.44132783753414f), Q31(-0.92688840659280f)}, {Q31(-0.85694974219574f), Q31( 0.88844532719844f)},
{Q31( 0.91783042091762f), Q31(-0.46356892383970f)}, {Q31( 0.72556974415690f), Q31(-0.99899555770747f)},
{Q31(-0.99711581834508f), Q31( 0.58211560180426f)}, {Q31( 0.77638976371966f), Q31( 0.94321834873819f)},
{Q31( 0.07717324253925f), Q31( 0.58638399856595f)}, {Q31(-0.56049829194163f), Q31( 0.82522301569036f)},
{Q31( 0.98398893639988f), Q31( 0.39467440420569f)}, {Q31( 0.47546946844938f), Q31( 0.68613044836811f)},
{Q31( 0.65675089314631f), Q31( 0.18331637134880f)}, {Q31( 0.03273375457980f), Q31(-0.74933109564108f)},
{Q31(-0.38684144784738f), Q31( 0.51337349030406f)}, {Q31(-0.97346267944545f), Q31(-0.96549364384098f)},
{Q31(-0.53282156061942f), Q31(-0.91423265091354f)}, {Q31( 0.99817310731176f), Q31( 0.61133572482148f)},
{Q31(-0.50254500772635f), Q31(-0.88829338134294f)}, {Q31( 0.01995873238855f), Q31( 0.85223515096765f)},
{Q31( 0.99930381973804f), Q31( 0.94578896296649f)}, {Q31( 0.82907767600783f), Q31(-0.06323442598128f)},
{Q31(-0.58660709669728f), Q31( 0.96840773806582f)}, {Q31(-0.17573736667267f), Q31(-0.48166920859485f)},
{Q31( 0.83434292401346f), Q31(-0.13023450646997f)}, {Q31( 0.05946491307025f), Q31( 0.20511047074866f)},
{Q31( 0.81505484574602f), Q31(-0.94685947861369f)}, {Q31(-0.44976380954860f), Q31( 0.40894572671545f)},
{Q31(-0.89746474625671f), Q31( 0.99846578838537f)}, {Q31( 0.39677256130792f), Q31(-0.74854668609359f)},
{Q31(-0.07588948563079f), Q31( 0.74096214084170f)}, {Q31( 0.76343198951445f), Q31( 0.41746629422634f)},
{Q31(-0.74490104699626f), Q31( 0.94725911744610f)}, {Q31( 0.64880119792759f), Q31( 0.41336660830571f)},
{Q31( 0.62319537462542f), Q31(-0.93098313552599f)}, {Q31( 0.42215817594807f), Q31(-0.07712787385208f)},
{Q31( 0.02704554141885f), Q31(-0.05417518053666f)}, {Q31( 0.80001773566818f), Q31( 0.91542195141039f)},
{Q31(-0.79351832348816f), Q31(-0.36208897989136f)}, {Q31( 0.63872359151636f), Q31( 0.08128252493444f)},
{Q31( 0.52890520960295f), Q31( 0.60048872455592f)}, {Q31( 0.74238552914587f), Q31( 0.04491915291044f)},
{Q31( 0.99096131449250f), Q31(-0.19451182854402f)}, {Q31(-0.80412329643109f), Q31(-0.88513818199457f)},
{Q31(-0.64612616129736f), Q31( 0.72198674804544f)}, {Q31( 0.11657770663191f), Q31(-0.83662833815041f)},
{Q31(-0.95053182488101f), Q31(-0.96939905138082f)}, {Q31(-0.62228872928622f), Q31( 0.82767262846661f)},
{Q31( 0.03004475787316f), Q31(-0.99738896333384f)}, {Q31(-0.97987214341034f), Q31( 0.36526129686425f)},
{Q31(-0.99986980746200f), Q31(-0.36021610299715f)}, {Q31( 0.89110648599879f), Q31(-0.97894250343044f)},
{Q31( 0.10407960510582f), Q31( 0.77357793811619f)}, {Q31( 0.95964737821728f), Q31(-0.35435818285502f)},
{Q31( 0.50843233159162f), Q31( 0.96107691266205f)}, {Q31( 0.17006334670615f), Q31(-0.76854025314829f)},
{Q31( 0.25872675063360f), Q31( 0.99893303933816f)}, {Q31(-0.01115998681937f), Q31( 0.98496019742444f)},
{Q31(-0.79598702973261f), Q31( 0.97138411318894f)}, {Q31(-0.99264708948101f), Q31(-0.99542822402536f)},
{Q31(-0.99829663752818f), Q31( 0.01877138824311f)}, {Q31(-0.70801016548184f), Q31( 0.33680685948117f)},
{Q31(-0.70467057786826f), Q31( 0.93272777501857f)}, {Q31( 0.99846021905254f), Q31(-0.98725746254433f)},
{Q31(-0.63364968534650f), Q31(-0.16473594423746f)}, {Q31(-0.16258217500792f), Q31(-0.95939125400802f)},
{Q31(-0.43645594360633f), Q31(-0.94805030113284f)}, {Q31(-0.99848471702976f), Q31( 0.96245166923809f)},
{Q31(-0.16796458968998f), Q31(-0.98987511890470f)}, {Q31(-0.87979225745213f), Q31(-0.71725725041680f)},
{Q31( 0.44183099021786f), Q31(-0.93568974498761f)}, {Q31( 0.93310180125532f), Q31(-0.99913308068246f)},
{Q31(-0.93941931782002f), Q31(-0.56409379640356f)}, {Q31(-0.88590003188677f), Q31( 0.47624600491382f)},
{Q31( 0.99971463703691f), Q31(-0.83889954253462f)}, {Q31(-0.75376385639978f), Q31( 0.00814643438625f)},
{Q31( 0.93887685615875f), Q31(-0.11284528204636f)}, {Q31( 0.85126435782309f), Q31( 0.52349251543547f)},
{Q31( 0.39701421446381f), Q31( 0.81779634174316f)}, {Q31(-0.37024464187437f), Q31(-0.87071656222959f)},
{Q31(-0.36024828242896f), Q31( 0.34655735648287f)}, {Q31(-0.93388812549209f), Q31(-0.84476541096429f)},
{Q31(-0.65298804552119f), Q31(-0.18439575450921f)}, {Q31( 0.11960319006843f), Q31( 0.99899346780168f)},
{Q31( 0.94292565553160f), Q31( 0.83163906518293f)}, {Q31( 0.75081145286948f), Q31(-0.35533223142265f)},
{Q31( 0.56721979748394f), Q31(-0.24076836414499f)}, {Q31( 0.46857766746029f), Q31(-0.30140233457198f)},
{Q31( 0.97312313923635f), Q31(-0.99548191630031f)}, {Q31(-0.38299976567017f), Q31( 0.98516909715427f)},
{Q31( 0.41025800019463f), Q31( 0.02116736935734f)}, {Q31( 0.09638062008048f), Q31( 0.04411984381457f)},
{Q31(-0.85283249275397f), Q31( 0.91475563922421f)}, {Q31( 0.88866808958124f), Q31(-0.99735267083226f)},
{Q31(-0.48202429536989f), Q31(-0.96805608884164f)}, {Q31( 0.27572582416567f), Q31( 0.58634753335832f)},
{Q31(-0.65889129659168f), Q31( 0.58835634138583f)}, {Q31( 0.98838086953732f), Q31( 0.99994349600236f)},
{Q31(-0.20651349620689f), Q31( 0.54593044066355f)}, {Q31(-0.62126416356920f), Q31(-0.59893681700392f)},
{Q31( 0.20320105410437f), Q31(-0.86879180355289f)}, {Q31(-0.97790548600584f), Q31( 0.96290806999242f)},
{Q31( 0.11112534735126f), Q31( 0.21484763313301f)}, {Q31(-0.41368337314182f), Q31( 0.28216837680365f)},
{Q31( 0.24133038992960f), Q31( 0.51294362630238f)}, {Q31(-0.66393410674885f), Q31(-0.08249679629081f)},
{Q31(-0.53697829178752f), Q31(-0.97649903936228f)}, {Q31(-0.97224737889348f), Q31( 0.22081333579837f)},
{Q31( 0.87392477144549f), Q31(-0.12796173740361f)}, {Q31( 0.19050361015753f), Q31( 0.01602615387195f)},
{Q31(-0.46353441212724f), Q31(-0.95249041539006f)}, {Q31(-0.07064096339021f), Q31(-0.94479803205886f)},
{Q31(-0.92444085484466f), Q31(-0.10457590187436f)}, {Q31(-0.83822593578728f), Q31(-0.01695043208885f)},
{Q31( 0.75214681811150f), Q31(-0.99955681042665f)}, {Q31(-0.42102998829339f), Q31( 0.99720941999394f)},
{Q31(-0.72094786237696f), Q31(-0.35008961934255f)}, {Q31( 0.78843311019251f), Q31( 0.52851398958271f)},
{Q31( 0.97394027897442f), Q31(-0.26695944086561f)}, {Q31( 0.99206463477946f), Q31(-0.57010120849429f)},
{Q31( 0.76789609461795f), Q31(-0.76519356730966f)}, {Q31(-0.82002421836409f), Q31(-0.73530179553767f)},
{Q31( 0.81924990025724f), Q31( 0.99698425250579f)}, {Q31(-0.26719850873357f), Q31( 0.68903369776193f)},
{Q31(-0.43311260380975f), Q31( 0.85321815947490f)}, {Q31( 0.99194979673836f), Q31( 0.91876249766422f)},
{Q31(-0.80692001248487f), Q31(-0.32627540663214f)}, {Q31( 0.43080003649976f), Q31(-0.21919095636638f)},
{Q31( 0.67709491937357f), Q31(-0.95478075822906f)}, {Q31( 0.56151770568316f), Q31(-0.70693811747778f)},
{Q31( 0.10831862810749f), Q31(-0.08628837174592f)}, {Q31( 0.91229417540436f), Q31(-0.65987351408410f)},
{Q31(-0.48972893932274f), Q31( 0.56289246362686f)}, {Q31(-0.89033658689697f), Q31(-0.71656563987082f)},
{Q31( 0.65269447475094f), Q31( 0.65916004833932f)}, {Q31( 0.67439478141121f), Q31(-0.81684380846796f)},
{Q31(-0.47770832416973f), Q31(-0.16789556203025f)}, {Q31(-0.99715979260878f), Q31(-0.93565784007648f)},
{Q31(-0.90889593602546f), Q31( 0.62034397054380f)}, {Q31(-0.06618622548177f), Q31(-0.23812217221359f)},
{Q31( 0.99430266919728f), Q31( 0.18812555317553f)}, {Q31( 0.97686402381843f), Q31(-0.28664534366620f)},
{Q31( 0.94813650221268f), Q31(-0.97506640027128f)}, {Q31(-0.95434497492853f), Q31(-0.79607978501983f)},
{Q31(-0.49104783137150f), Q31( 0.32895214359663f)}, {Q31( 0.99881175120751f), Q31( 0.88993983831354f)},
{Q31( 0.50449166760303f), Q31(-0.85995072408434f)}, {Q31( 0.47162891065108f), Q31(-0.18680204049569f)},
{Q31(-0.62081581361840f), Q31( 0.75000676218956f)}, {Q31(-0.43867015250812f), Q31( 0.99998069244322f)},
{Q31( 0.98630563232075f), Q31(-0.53578899600662f)}, {Q31(-0.61510362277374f), Q31(-0.89515019899997f)},
{Q31(-0.03841517601843f), Q31(-0.69888815681179f)}, {Q31(-0.30102157304644f), Q31(-0.07667808922205f)},
{Q31( 0.41881284182683f), Q31( 0.02188098922282f)}, {Q31(-0.86135454941237f), Q31( 0.98947480909359f)},
{Q31( 0.67226861393788f), Q31(-0.13494389011014f)}, {Q31(-0.70737398842068f), Q31(-0.76547349325992f)},
{Q31( 0.94044946687963f), Q31( 0.09026201157416f)}, {Q31(-0.82386352534327f), Q31( 0.08924768823676f)},
{Q31(-0.32070666698656f), Q31( 0.50143421908753f)}, {Q31( 0.57593163224487f), Q31(-0.98966422921509f)},
{Q31(-0.36326018419965f), Q31( 0.07440243123228f)}, {Q31( 0.99979044674350f), Q31(-0.14130287347405f)},
{Q31(-0.92366023326932f), Q31(-0.97979298068180f)}, {Q31(-0.44607178518598f), Q31(-0.54233252016394f)},
{Q31( 0.44226800932956f), Q31( 0.71326756742752f)}, {Q31( 0.03671907158312f), Q31( 0.63606389366675f)},
{Q31( 0.52175424682195f), Q31(-0.85396826735705f)}, {Q31(-0.94701139690956f), Q31(-0.01826348194255f)},
{Q31(-0.98759606946049f), Q31( 0.82288714303073f)}, {Q31( 0.87434794743625f), Q31( 0.89399495655433f)},
{Q31(-0.93412041758744f), Q31( 0.41374052024363f)}, {Q31( 0.96063943315511f), Q31( 0.93116709541280f)},
{Q31( 0.97534253457837f), Q31( 0.86150930812689f)}, {Q31( 0.99642466504163f), Q31( 0.70190043427512f)},
{Q31(-0.94705089665984f), Q31(-0.29580042814306f)}, {Q31( 0.91599807087376f), Q31(-0.98147830385781f)},
// Start of duplicated table
{Q31(-0.99948153278296f), Q31(-0.59483417516607f)}, {Q31( 0.97113454393991f), Q31(-0.67528515225647f)},
{Q31( 0.14130051758487f), Q31(-0.95090983575689f)}, {Q31(-0.47005496701697f), Q31(-0.37340549728647f)},
{Q31( 0.80705063769351f), Q31( 0.29653668284408f)}, {Q31(-0.38981478896926f), Q31( 0.89572605717087f)},
{Q31(-0.01053049862020f), Q31(-0.66959058036166f)}, {Q31(-0.91266367957293f), Q31(-0.11522938140034f)},
#if ARCH_RISCV
{Q31( 0.54840422910309f), Q31( 0.75221367176302f)}, {Q31( 0.40009252867955f), Q31(-0.98929400334421f)},
{Q31(-0.99867974711855f), Q31(-0.88147068645358f)}, {Q31(-0.95531076805040f), Q31( 0.90908757154593f)},
{Q31(-0.45725933317144f), Q31(-0.56716323646760f)}, {Q31(-0.72929675029275f), Q31(-0.98008272727324f)},
{Q31( 0.75622801399036f), Q31( 0.20950329995549f)}, {Q31( 0.07069442601050f), Q31(-0.78247898470706f)},
{Q31( 0.74496252926055f), Q31(-0.91169004445807f)}, {Q31(-0.96440182703856f), Q31(-0.94739918296622f)},
{Q31( 0.30424629369539f), Q31(-0.49438267012479f)}, {Q31( 0.66565033746925f), Q31( 0.64652935542491f)},
{Q31( 0.91697008020594f), Q31( 0.17514097332009f)}, {Q31(-0.70774918760427f), Q31( 0.52548653416543f)},
{Q31(-0.70051415345560f), Q31(-0.45340028808763f)}, {Q31(-0.99496513054797f), Q31(-0.90071908066973f)},
{Q31( 0.98164490790123f), Q31(-0.77463155528697f)}, {Q31(-0.54671580548181f), Q31(-0.02570928536004f)},
{Q31(-0.01689629065389f), Q31( 0.00287506445732f)}, {Q31(-0.86110349531986f), Q31( 0.42548583726477f)},
{Q31(-0.98892980586032f), Q31(-0.87881132267556f)}, {Q31( 0.51756627678691f), Q31( 0.66926784710139f)},
{Q31(-0.99635026409640f), Q31(-0.58107730574765f)}, {Q31(-0.99969370862163f), Q31( 0.98369989360250f)},
#endif
};
