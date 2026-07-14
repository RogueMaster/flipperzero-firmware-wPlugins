
#include "gs1_parser_i.h"

#define LOG_TAG "gs1_rfid_parser_gs1_parser_ai"

static int32_t parse_ai(
    GS1_AI* ai,
    const uint8_t* data_stream,
    uint32_t starting_bit,
    uint32_t data_bit_count) {
    FURI_LOG_T(LOG_TAG, __func__);

    int32_t batch_size;
    int32_t ai_length = 8;
    ai->ai_identifier = read_next_n_bits(data_stream, starting_bit, 8);

    // Some AIs are longer than 2 characters.
    // This is to handle those special cases to read the full AI.
    switch(ai->ai_identifier) {
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x40:
    case 0x41:
    case 0x42:
    case 0x71:
        ai_length = 12;
        ai->ai_identifier = read_next_n_bits(data_stream, starting_bit, 12);
        break;
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x35:
    case 0x36:
    case 0x39:
    case 0x43:
    case 0x70:
    case 0x72:
    case 0x80:
    case 0x81:
    case 0x82:
        ai_length = 16;
        ai->ai_identifier = read_next_n_bits(data_stream, starting_bit, 16);
    }

    FURI_LOG_D(LOG_TAG, "Parsing AI %X", ai->ai_identifier);

    switch(ai->ai_identifier) {
    // 18 digit fixed length numeric
    case 0x00:
    case 0x8006:
    case 0x8017:
    case 0x8018:
    case 0x8026:
        ai_length +=
            parse_fixed_length_numeric(data_stream, starting_bit + ai_length, 18, ai->ai_value);
        break;
    // 14 digit fixed length numeric
    case 0x01:
    case 0x02:
        ai_length +=
            parse_fixed_length_numeric(data_stream, starting_bit + ai_length, 14, ai->ai_value);
        break;
    // 14 digit fixed length numeric and variable length alphanumeric
    case 0x8003:
        ai_length +=
            parse_fixed_length_numeric(data_stream, starting_bit + ai_length, 14, ai->ai_value);
        ai->ai_value[14] = ',';

        ai_length += parse_fixed_length_numeric(
            data_stream, starting_bit + ai_length, 14, &ai->ai_value[15]);
        break;
    // 13 digit fixed length numeric
    case 0x410:
    case 0x411:
    case 0x412:
    case 0x413:
    case 0x414:
    case 0x415:
    case 0x416:
    case 0x417:
        ai_length +=
            parse_fixed_length_numeric(data_stream, starting_bit + ai_length, 13, ai->ai_value);
        break;
    // 2 digit fixed length numeric
    case 0x7241:
        ai_length +=
            parse_fixed_length_numeric(data_stream, starting_bit + ai_length, 2, ai->ai_value);
        break;
    // 13 digit fixed length numeric with a variable length alphanumeric
    case 0x253:
        ai_length +=
            parse_fixed_length_numeric(data_stream, starting_bit + ai_length, 13, ai->ai_value);
        ai->ai_value[13] = ',';

        batch_size = parse_variable_length_alphanumeric(
            data_stream, starting_bit + ai_length, 5, &ai->ai_value[14]);

        if(batch_size < 0) {
            return PARSING_MALFORMED_DATA;
        }
        ai_length += batch_size;
        break;
    // 13 digit fixed length numeric with a 4 bit variable length integer
    case 0x255:
        ai_length +=
            parse_fixed_length_numeric(data_stream, starting_bit + ai_length, 13, ai->ai_value);
        ai->ai_value[13] = ',';
        ai_length += parse_variable_length_numeric(
            data_stream, starting_bit + ai_length, 4, &ai->ai_value[14]);
        break;
    // Variable length alpha numeric with an length bit count of 5
    case 0x10:
    case 0x21:
    case 0x22:
    case 0x235:
    case 0x240:
    case 0x241:
    case 0x243:
    case 0x250:
    case 0x251:
    case 0x254:
    case 0x400:
    case 0x401:
    case 0x403:
    case 0x420:
    case 0x4308:
    case 0x4318:
    case 0x4319:
    case 0x7002:
    case 0x7020:
    case 0x7021:
    case 0x7022:
    case 0x7023:
    case 0x710:
    case 0x711:
    case 0x712:
    case 0x713:
    case 0x714:
    case 0x715:
    case 0x716:
    case 0x7230:
    case 0x7231:
    case 0x7232:
    case 0x7233:
    case 0x7234:
    case 0x7235:
    case 0x7236:
    case 0x7237:
    case 0x7238:
    case 0x7239:
    case 0x7240:
    case 0x7242:
    case 0x8002:
    case 0x8007:
    case 0x8012:
    case 0x8013:
    case 0x8020:
    case 0x90:
        batch_size = parse_variable_length_alphanumeric(
            data_stream, starting_bit + ai_length, 5, ai->ai_value);

        if(batch_size < 0) {
            return PARSING_MALFORMED_DATA;
        }
        ai_length += batch_size;
        break;
    // Variable length alpha numeric with an length bit count of 2
    case 0x427:
    case 0x7008:
    case 0x7010:
        batch_size = parse_variable_length_alphanumeric(
            data_stream, starting_bit + ai_length, 2, ai->ai_value);

        if(batch_size < 0) {
            return PARSING_MALFORMED_DATA;
        }
        ai_length += batch_size;
        break;
    // Variable length alpha numeric with an length bit count of 3
    case 0x7040:
    case 0x7041:
        batch_size = parse_variable_length_alphanumeric(
            data_stream, starting_bit + ai_length, 3, ai->ai_value);

        if(batch_size < 0) {
            return PARSING_MALFORMED_DATA;
        }
        ai_length += batch_size;
        break;
    // Variable length alpha numeric with an length bit count of 4
    case 0x7005:
    case 0x7009:
    case 0x7255:
        batch_size = parse_variable_length_alphanumeric(
            data_stream, starting_bit + ai_length, 4, ai->ai_value);

        if(batch_size < 0) {
            return PARSING_MALFORMED_DATA;
        }
        ai_length += batch_size;
        break;
    // Variable length alpha numeric with an length bit count of 6
    case 0x4300:
    case 0x4301:
    case 0x4310:
    case 0x4311:
    case 0x4320:
    case 0x7253:
    case 0x7254:
    case 0x7259:
    case 0x8009:
        batch_size = parse_variable_length_alphanumeric(
            data_stream, starting_bit + ai_length, 6, ai->ai_value);

        if(batch_size < 0) {
            return PARSING_MALFORMED_DATA;
        }
        ai_length += batch_size;
        break;
    // Variable length alpha numeric with an length bit count of 7
    case 0x4302:
    case 0x4303:
    case 0x4304:
    case 0x4305:
    case 0x4306:
    case 0x4312:
    case 0x4313:
    case 0x4314:
    case 0x4315:
    case 0x4316:
    case 0x7256:
    case 0x7257:
    case 0x8030:
    case 0x8110:
    case 0x8112:
    case 0x8200:
    case 0x91:
    case 0x92:
    case 0x93:
    case 0x94:
    case 0x95:
    case 0x96:
    case 0x97:
    case 0x98:
    case 0x99:
        batch_size = parse_variable_length_alphanumeric(
            data_stream, starting_bit + ai_length, 7, ai->ai_value);

        if(batch_size < 0) {
            return PARSING_MALFORMED_DATA;
        }
        ai_length += batch_size;
        break;
    // Variable date format
    case 0x7007:
        if(read_next_n_bits(data_stream, starting_bit + ai_length, 1)) {
            snprintf(
                ai->ai_value,
                AI_VALUE_LENGTH,
                "%02lld/%02lld/%02lld - %02lld/%02lld/%02lld",
                read_next_n_bits(data_stream, starting_bit + ai_length + 12, 5), // Day
                read_next_n_bits(data_stream, starting_bit + ai_length + 8, 4), // Month
                read_next_n_bits(data_stream, starting_bit + ai_length + 1, 7), // Year
                read_next_n_bits(data_stream, starting_bit + ai_length + 28, 5), // Day
                read_next_n_bits(data_stream, starting_bit + ai_length + 24, 4), // Month
                read_next_n_bits(data_stream, starting_bit + ai_length + 17, 7) // Year
            );
            ai_length += 33;
            break;
        } else {
            snprintf(
                ai->ai_value,
                AI_VALUE_LENGTH,
                "%02lld/%02lld/%02lld",
                read_next_n_bits(data_stream, starting_bit + ai_length + 12, 5), // Day
                read_next_n_bits(data_stream, starting_bit + ai_length + 8, 4), // Month
                read_next_n_bits(data_stream, starting_bit + ai_length + 1, 7) // Year
            );
            ai_length += 17;
            break;
        }
    // 6 digit date format (YYMMDD)
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x4326:
    case 0x7006:
        snprintf(
            ai->ai_value,
            AI_VALUE_LENGTH,
            "%02lld/%02lld/%02lld",
            read_next_n_bits(data_stream, starting_bit + ai_length + 11, 5), // Day
            read_next_n_bits(data_stream, starting_bit + ai_length + 7, 4), // Month
            read_next_n_bits(data_stream, starting_bit + ai_length, 7) // Year
        );
        ai_length += 16;
        break;
    // 10 digit date and time format (YYMMDDhhmm)
    case 0x4324:
    case 0x4325:
    case 0x7003:
        snprintf(
            ai->ai_value,
            AI_VALUE_LENGTH,
            "%02lld/%02lld/%02lld %02lld:%02lld",
            read_next_n_bits(data_stream, starting_bit + ai_length + 11, 5), // Day
            read_next_n_bits(data_stream, starting_bit + ai_length + 7, 4), // Month
            read_next_n_bits(data_stream, starting_bit + ai_length, 7), // Year
            read_next_n_bits(data_stream, starting_bit + ai_length + 16, 5), // Hour
            read_next_n_bits(data_stream, starting_bit + ai_length + 21, 6) // Minute
        );
        ai_length += 27;
        break;
    // Variable precision date time
    case 0x7011:
    case 0x8008:
        uint8_t variable_precision_format =
            read_next_n_bits(data_stream, starting_bit + ai_length, 2);
        ai_length += 2;
        if(variable_precision_format == 0) {
            snprintf(
                ai->ai_value,
                AI_VALUE_LENGTH,
                "%02lld/%02lld/%02lld %02lld:00",
                read_next_n_bits(data_stream, starting_bit + ai_length + 11, 5), // Day
                read_next_n_bits(data_stream, starting_bit + ai_length + 7, 4), // Month
                read_next_n_bits(data_stream, starting_bit + ai_length, 7), // Year
                read_next_n_bits(data_stream, starting_bit + ai_length + 16, 5) // Hour
            );
            ai_length += 21;
        } else if(variable_precision_format == 1) {
            snprintf(
                ai->ai_value,
                AI_VALUE_LENGTH,
                "%02lld/%02lld/%02lld %02lld:%02lld",
                read_next_n_bits(data_stream, starting_bit + ai_length + 11, 5), // Day
                read_next_n_bits(data_stream, starting_bit + ai_length + 7, 4), // Month
                read_next_n_bits(data_stream, starting_bit + ai_length, 7), // Year
                read_next_n_bits(data_stream, starting_bit + ai_length + 16, 5), // Hour
                read_next_n_bits(data_stream, starting_bit + ai_length + 21, 6) // Minute
            );
            ai_length += 27;
        } else if(variable_precision_format == 2) {
            snprintf(
                ai->ai_value,
                AI_VALUE_LENGTH,
                "%02lld/%02lld/%02lld %02lld:%02lld:%02lld",
                read_next_n_bits(data_stream, starting_bit + ai_length + 11, 5), // Day
                read_next_n_bits(data_stream, starting_bit + ai_length + 7, 4), // Month
                read_next_n_bits(data_stream, starting_bit + ai_length, 7), // Year
                read_next_n_bits(data_stream, starting_bit + ai_length + 16, 5), // Hour
                read_next_n_bits(data_stream, starting_bit + ai_length + 21, 6), // Minute
                read_next_n_bits(data_stream, starting_bit + ai_length + 27, 6) // Second
            );
            ai_length += 33;
        } else {
            snprintf(
                ai->ai_value,
                AI_VALUE_LENGTH,
                "%02lld/%02lld/%02lld",
                read_next_n_bits(data_stream, starting_bit + ai_length + 11, 5), // Day
                read_next_n_bits(data_stream, starting_bit + ai_length + 7, 4), // Month
                read_next_n_bits(data_stream, starting_bit + ai_length, 7) // Year
            );
            ai_length += 16;
        }
        break;
    // 4 bit fixed bit length integer
    case 0x7252:
        snprintf(
            ai->ai_value,
            AI_VALUE_LENGTH,
            "%lld",
            read_next_n_bits(data_stream, starting_bit + ai_length, 4));
        ai_length += 4;
        break;
    // 7 bit fixed bit length integer
    case 0x20:
        snprintf(
            ai->ai_value,
            AI_VALUE_LENGTH,
            "%02lld",
            read_next_n_bits(data_stream, starting_bit + ai_length, 7));
        ai_length += 7;
        break;
    // 20 bit fixed bit length integer
    case 0x3100:
    case 0x3101:
    case 0x3102:
    case 0x3103:
    case 0x3104:
    case 0x3105:
    case 0x3110:
    case 0x3111:
    case 0x3112:
    case 0x3113:
    case 0x3114:
    case 0x3115:
    case 0x3120:
    case 0x3121:
    case 0x3122:
    case 0x3123:
    case 0x3124:
    case 0x3125:
    case 0x3130:
    case 0x3131:
    case 0x3132:
    case 0x3133:
    case 0x3134:
    case 0x3135:
    case 0x3140:
    case 0x3141:
    case 0x3142:
    case 0x3143:
    case 0x3144:
    case 0x3145:
    case 0x3150:
    case 0x3151:
    case 0x3152:
    case 0x3153:
    case 0x3154:
    case 0x3155:
    case 0x3160:
    case 0x3161:
    case 0x3162:
    case 0x3163:
    case 0x3164:
    case 0x3165:
    case 0x3200:
    case 0x3201:
    case 0x3202:
    case 0x3203:
    case 0x3204:
    case 0x3205:
    case 0x3210:
    case 0x3211:
    case 0x3212:
    case 0x3213:
    case 0x3214:
    case 0x3215:
    case 0x3220:
    case 0x3221:
    case 0x3222:
    case 0x3223:
    case 0x3224:
    case 0x3225:
    case 0x3230:
    case 0x3231:
    case 0x3232:
    case 0x3233:
    case 0x3234:
    case 0x3235:
    case 0x3240:
    case 0x3241:
    case 0x3242:
    case 0x3243:
    case 0x3244:
    case 0x3245:
    case 0x3250:
    case 0x3251:
    case 0x3252:
    case 0x3253:
    case 0x3254:
    case 0x3255:
    case 0x3260:
    case 0x3261:
    case 0x3262:
    case 0x3263:
    case 0x3264:
    case 0x3265:
    case 0x3270:
    case 0x3271:
    case 0x3272:
    case 0x3273:
    case 0x3274:
    case 0x3275:
    case 0x3280:
    case 0x3281:
    case 0x3282:
    case 0x3283:
    case 0x3284:
    case 0x3285:
    case 0x3290:
    case 0x3291:
    case 0x3292:
    case 0x3293:
    case 0x3294:
    case 0x3295:
    case 0x3300:
    case 0x3301:
    case 0x3302:
    case 0x3303:
    case 0x3304:
    case 0x3305:
    case 0x3310:
    case 0x3311:
    case 0x3312:
    case 0x3313:
    case 0x3314:
    case 0x3315:
    case 0x3320:
    case 0x3321:
    case 0x3322:
    case 0x3323:
    case 0x3324:
    case 0x3325:
    case 0x3330:
    case 0x3331:
    case 0x3332:
    case 0x3333:
    case 0x3334:
    case 0x3335:
    case 0x3340:
    case 0x3341:
    case 0x3342:
    case 0x3343:
    case 0x3344:
    case 0x3345:
    case 0x3350:
    case 0x3351:
    case 0x3352:
    case 0x3353:
    case 0x3354:
    case 0x3355:
    case 0x3360:
    case 0x3361:
    case 0x3362:
    case 0x3363:
    case 0x3364:
    case 0x3365:
    case 0x3370:
    case 0x3371:
    case 0x3372:
    case 0x3373:
    case 0x3374:
    case 0x3375:
    case 0x3400:
    case 0x3401:
    case 0x3402:
    case 0x3403:
    case 0x3404:
    case 0x3405:
    case 0x3410:
    case 0x3411:
    case 0x3412:
    case 0x3413:
    case 0x3414:
    case 0x3415:
    case 0x3420:
    case 0x3421:
    case 0x3422:
    case 0x3423:
    case 0x3424:
    case 0x3425:
    case 0x3430:
    case 0x3431:
    case 0x3432:
    case 0x3433:
    case 0x3434:
    case 0x3435:
    case 0x3440:
    case 0x3441:
    case 0x3442:
    case 0x3443:
    case 0x3444:
    case 0x3445:
    case 0x3450:
    case 0x3451:
    case 0x3452:
    case 0x3453:
    case 0x3454:
    case 0x3455:
    case 0x3460:
    case 0x3461:
    case 0x3462:
    case 0x3463:
    case 0x3464:
    case 0x3465:
    case 0x3470:
    case 0x3471:
    case 0x3472:
    case 0x3473:
    case 0x3474:
    case 0x3475:
    case 0x3480:
    case 0x3481:
    case 0x3482:
    case 0x3483:
    case 0x3484:
    case 0x3485:
    case 0x3490:
    case 0x3491:
    case 0x3492:
    case 0x3493:
    case 0x3494:
    case 0x3495:
    case 0x3500:
    case 0x3501:
    case 0x3502:
    case 0x3503:
    case 0x3504:
    case 0x3505:
    case 0x3510:
    case 0x3511:
    case 0x3512:
    case 0x3513:
    case 0x3514:
    case 0x3515:
    case 0x3520:
    case 0x3521:
    case 0x3522:
    case 0x3523:
    case 0x3524:
    case 0x3525:
    case 0x3530:
    case 0x3531:
    case 0x3532:
    case 0x3533:
    case 0x3534:
    case 0x3535:
    case 0x3540:
    case 0x3541:
    case 0x3542:
    case 0x3543:
    case 0x3544:
    case 0x3545:
    case 0x3550:
    case 0x3551:
    case 0x3552:
    case 0x3553:
    case 0x3554:
    case 0x3555:
    case 0x3560:
    case 0x3561:
    case 0x3562:
    case 0x3563:
    case 0x3564:
    case 0x3565:
    case 0x3570:
    case 0x3571:
    case 0x3572:
    case 0x3573:
    case 0x3574:
    case 0x3575:
    case 0x3600:
    case 0x3601:
    case 0x3602:
    case 0x3603:
    case 0x3604:
    case 0x3605:
    case 0x3610:
    case 0x3611:
    case 0x3612:
    case 0x3613:
    case 0x3614:
    case 0x3615:
    case 0x3620:
    case 0x3621:
    case 0x3622:
    case 0x3623:
    case 0x3624:
    case 0x3625:
    case 0x3630:
    case 0x3631:
    case 0x3632:
    case 0x3633:
    case 0x3634:
    case 0x3635:
    case 0x3640:
    case 0x3641:
    case 0x3642:
    case 0x3643:
    case 0x3644:
    case 0x3645:
    case 0x3650:
    case 0x3651:
    case 0x3652:
    case 0x3653:
    case 0x3654:
    case 0x3655:
    case 0x3660:
    case 0x3661:
    case 0x3662:
    case 0x3663:
    case 0x3664:
    case 0x3665:
    case 0x3670:
    case 0x3671:
    case 0x3672:
    case 0x3673:
    case 0x3674:
    case 0x3675:
    case 0x3680:
    case 0x3681:
    case 0x3682:
    case 0x3683:
    case 0x3684:
    case 0x3685:
    case 0x3690:
    case 0x3691:
    case 0x3692:
    case 0x3693:
    case 0x3694:
    case 0x3695:
    case 0x3950:
    case 0x3951:
    case 0x3952:
    case 0x3953:
    case 0x8005:
        snprintf(
            ai->ai_value,
            AI_VALUE_LENGTH,
            "%06lld",
            read_next_n_bits(data_stream, starting_bit + ai_length, 20));
        ai_length += 20;
        break;
    // 20 bit fixed bit length integer with optional signed bit
    case 0x4330:
    case 0x4331:
    case 0x4332:
    case 0x4333:
        snprintf(
            ai->ai_value,
            AI_VALUE_LENGTH,
            "%06lld",
            read_next_n_bits(data_stream, starting_bit + ai_length, 20) *
                (read_next_n_bits(data_stream, starting_bit + ai_length + 20, 1) ? -1 : 1));
        ai_length += 21;
        break;
    // 14 bit fixed bit length integer
    case 0x3940:
    case 0x3941:
    case 0x3942:
    case 0x3943:
    case 0x8111:
        snprintf(
            ai->ai_value,
            AI_VALUE_LENGTH,
            "%04lld",
            read_next_n_bits(data_stream, starting_bit + ai_length, 14));
        ai_length += 14;
        break;
    // 27 bit fixed bit length integer
    case 0x7250:
        snprintf(
            ai->ai_value,
            AI_VALUE_LENGTH,
            "%08lld",
            read_next_n_bits(data_stream, starting_bit + ai_length, 27));
        ai_length += 27;
        break;
    // 40 bit fixed bit length integer
    case 0x7251:
        snprintf(
            ai->ai_value,
            AI_VALUE_LENGTH,
            "%012lld",
            read_next_n_bits(data_stream, starting_bit + ai_length, 40));
        ai_length += 40;
        break;
    // 44 bit fixed bit length integer
    case 0x7001:
        snprintf(
            ai->ai_value,
            AI_VALUE_LENGTH,
            "%013lld",
            read_next_n_bits(data_stream, starting_bit + ai_length, 44));
        ai_length += 44;
        break;
    // 47 bit fixed bit length integer
    case 0x8001:
        snprintf(
            ai->ai_value,
            AI_VALUE_LENGTH,
            "%014lld",
            read_next_n_bits(data_stream, starting_bit + ai_length, 47));
        ai_length += 47;
        break;
    // 57 bit fixed bit length integer
    case 0x402:
        snprintf(
            ai->ai_value,
            AI_VALUE_LENGTH,
            "%017lld",
            read_next_n_bits(data_stream, starting_bit + ai_length, 57));
        ai_length += 57;
        break;
    // 67 bit fixed bit length integer
    case 0x4309:
        snprintf(
            ai->ai_value,
            AI_VALUE_LENGTH,
            "%020lld",
            read_next_n_bits(data_stream, starting_bit + ai_length, 67));
        ai_length += 67;
        break;
    // 10 bit fixed bit length integer and 4 bit variable length integer
    case 0x3910:
    case 0x3911:
    case 0x3912:
    case 0x3913:
    case 0x3914:
    case 0x3915:
    case 0x3916:
    case 0x3917:
    case 0x3918:
    case 0x3919:
    case 0x3930:
    case 0x3931:
    case 0x3932:
    case 0x3933:
    case 0x3934:
    case 0x3935:
    case 0x3936:
    case 0x3937:
    case 0x3938:
    case 0x3939:
        snprintf(
            ai->ai_value,
            AI_VALUE_LENGTH,
            "%03lld",
            read_next_n_bits(data_stream, starting_bit + ai_length, 10));
        ai_length += 10;
        ai->ai_value[3] = ',';
        ai_length += parse_variable_length_numeric(
            data_stream, starting_bit + ai_length, 3, &ai->ai_value[4]);
        break;
    // 10 bit fixed bit length integer and 5 bit variable length alphanumeric
    case 0x7030:
    case 0x7031:
    case 0x7032:
    case 0x7033:
    case 0x7034:
    case 0x7035:
    case 0x7036:
    case 0x7037:
    case 0x7038:
    case 0x7039:
        snprintf(
            ai->ai_value,
            AI_VALUE_LENGTH,
            "%03lld",
            read_next_n_bits(data_stream, starting_bit + ai_length, 10));
        ai_length += 10;
        ai->ai_value[3] = ',';
        ai_length += parse_variable_length_alphanumeric(
            data_stream, starting_bit + ai_length, 5, &ai->ai_value[4]);
        break;
    // 10 bit fixed bit length integer and 4 bit variable length alphanumeric
    case 0x421:
        snprintf(
            ai->ai_value,
            AI_VALUE_LENGTH,
            "%03lld",
            read_next_n_bits(data_stream, starting_bit + ai_length, 10));
        ai_length += 10;
        ai->ai_value[3] = ',';
        ai_length += parse_variable_length_alphanumeric(
            data_stream, starting_bit + ai_length, 4, &ai->ai_value[4]);
        break;
    // 10 bit fixed bit length integer
    case 0x422:
    case 0x424:
    case 0x426:
        snprintf(
            ai->ai_value,
            AI_VALUE_LENGTH,
            "%03lld",
            read_next_n_bits(data_stream, starting_bit + ai_length, 10));
        ai_length += 10;
        break;
    // 3 bit variable length integer without encoding
    case 0x242:
    case 0x7004:
        ai_length +=
            parse_variable_length_numeric(data_stream, starting_bit + ai_length, 3, ai->ai_value);
        break;
    // 4 bit variable length integer without encoding
    case 0x30:
    case 0x37:
    case 0x3900:
    case 0x3901:
    case 0x3902:
    case 0x3903:
    case 0x3904:
    case 0x3905:
    case 0x3906:
    case 0x3907:
    case 0x3908:
    case 0x3909:
    case 0x3920:
    case 0x3921:
    case 0x3922:
    case 0x3923:
    case 0x3924:
    case 0x3925:
    case 0x3926:
    case 0x3927:
    case 0x3928:
    case 0x3929:
    case 0x423:
    case 0x425:
    case 0x8011:
    case 0x8019:
        ai_length +=
            parse_variable_length_numeric(data_stream, starting_bit + ai_length, 4, ai->ai_value);
        break;
    // Country Code
    case 0x4307:
    case 0x4317:
        ai_length += parse_country_code(data_stream, starting_bit + ai_length, ai->ai_value);
        break;
    // Single data bit
    case 0x4321:
    case 0x4322:
    case 0x4323:
        snprintf(
            ai->ai_value,
            AI_VALUE_LENGTH,
            "%s",
            read_next_n_bits(data_stream, starting_bit + ai_length, 1) ? "True" : "False");
        ai_length += 1;
        break;
    // Delimited or terminated numeric
    case 0x8004:
    case 0x8010:
        uint8_t length = 0;
        while(true) {
            uint64_t next_nybble = read_next_n_bits(data_stream, starting_bit + ai_length, 4);

            if(next_nybble == 0xF) {
                ai->ai_value[length] = '\0';
                ai_length += 4;
                break;
            } else if(next_nybble == 0xE) {
                ai_length += 4;
                ai_length += parse_variable_length_alphanumeric(
                    data_stream, starting_bit + ai_length, 5, &ai->ai_value[length]);
                break;
            } else {
                ai->ai_value[length] = '0' + (next_nybble & 0xF);
                length++;
                ai_length += 4;
            }
        }
        break;
    // Baby birth sequence identifier
    case 0x7258:
        snprintf(
            ai->ai_value,
            AI_VALUE_LENGTH,
            "%lld/%lld",
            read_next_n_bits(data_stream, starting_bit + ai_length, 4),
            read_next_n_bits(data_stream, starting_bit + ai_length + 4, 4));
        ai_length += 8;
        break;
    default:
        snprintf(ai->ai_value, AI_VALUE_LENGTH, "Not Implemented");
        return data_bit_count - starting_bit;
    }

    ai->ai_value[AI_VALUE_LENGTH - 1] = '\0';
    FURI_LOG_I(LOG_TAG, "AI %02X: %s", ai->ai_identifier, ai->ai_value);
    return ai_length;
}

ParsingResult parse_epc_ais(
    ParsedTagInformation* tag_info,
    const uint8_t* data_stream,
    uint32_t starting_bit,
    uint32_t data_bit_count) {
    FURI_LOG_T(LOG_TAG, __func__);

    while(starting_bit + 8 < data_bit_count) {
        int32_t ai_length = parse_ai(
            &(tag_info->ai_list[tag_info->ai_count]), data_stream, starting_bit, data_bit_count);

        if(ai_length < 0) {
            return PARSING_MALFORMED_DATA;
        }

        starting_bit += ai_length;
        tag_info->ai_count++;
    }

    FURI_LOG_I(LOG_TAG, "Parsed %d AIs", tag_info->ai_count);
    return PARSING_SUCCESS;
}
