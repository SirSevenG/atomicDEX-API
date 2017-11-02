
/******************************************************************************
 * Copyright © 2014-2017 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/
//
//  LP_tradebots.c
//  marketmaker
//

#define TRADEBOTS_GAPTIME 60
#define LP_TRADEBOTS_MAXTRADES 100

struct LP_tradebot_trade
{
    double maxprice,totalrelvolume,basevol,relvol;
    uint64_t aliceid;
    int32_t dispdir;
    uint32_t started,finished,requestid,quoteid,tradeid;
    char base[32],rel[32];
};

struct LP_tradebot
{
    struct LP_tradebot *next,*prev;
    char name[128],base[32],rel[32];
    int32_t numtrades,numpending,completed,dispdir;
    double maxprice,totalrelvolume,totalbasevolume,basesum,relsum,pendbasesum,pendrelsum;
    uint32_t dead,pause,started,id;
    struct LP_tradebot_trade *trades[LP_TRADEBOTS_MAXTRADES];
} *LP_tradebots;

void LP_tradebot_updatestats(struct LP_tradebot *bot,struct LP_tradebot_trade *tp)
{
    char *swapstr,*status; int32_t flag; cJSON *swapjson;
    if ( (swapstr= basilisk_swapentry(tp->requestid,tp->quoteid)) != 0 )
    {
        flag = 0;
        if ( (swapjson= cJSON_Parse(swapstr)) != 0 )
        {
            tp->basevol = dstr(j64bits(swapjson,"satoshis"));
            tp->relvol = dstr(j64bits(swapjson,"destsatoshis"));
            tp->aliceid = j64bits(swapjson,"aliceid");
            if ( (status= jstr(swapjson,"status")) != 0 )
            {
                if ( strcmp(status,"finished") == 0 )
                {
                    flag = 1;
                    bot->completed++;
                    bot->basesum += tp->basevol;
                    bot->relsum += tp->relvol;
                }
            }
            if ( flag == 0 )
            {
                bot->numpending++;
                bot->pendbasesum += tp->basevol;
                bot->pendrelsum += tp->relvol;
            }
            free_json(swapjson);
        }
        free(swapstr);
    }
}

void LP_tradebot_calcstats(struct LP_tradebot *bot)
{
    int32_t i;
    bot->basesum = bot->relsum = bot->pendbasesum = bot->pendrelsum = 0.;
    bot->numpending = bot->completed = 0;
    for (i=0; i<bot->numtrades; i++)
        LP_tradebot_updatestats(bot,bot->trades[i]);
}

double LP_pricevol_invert(double *basevolumep,double maxprice,double relvolume)
{
    double price;
    *basevolumep = 0.;
    if ( maxprice > SMALLVAL && maxprice < SATOSHIDEN )
    {
        price = (1. / maxprice);
        *basevolumep = (relvolume * price);
        return(price);
    }
    return(0.);
}

cJSON *LP_tradebot_tradejson(struct LP_tradebot_trade *tp,int32_t dispflag)
{
    double price,basevol; cJSON *item = cJSON_CreateObject();
    if ( tp == 0 )
        return(cJSON_Parse("{}"));
    if ( tp->requestid != 0 && tp->quoteid != 0 )
    {
        jaddnum(item,"requestid",tp->requestid);
        jaddnum(item,"quoteid",tp->quoteid);
    } else jaddnum(item,"tradeid",tp->tradeid);
    if ( tp->basevol > SMALLVAL && tp->relvol > SMALLVAL )
    {
        if ( dispflag > 0 )
        {
            jaddnum(item,"price",tp->relvol/tp->basevol);
            jaddnum(item,"volume",tp->relvol);
        }
        else
        {
            price = LP_pricevol_invert(&basevol,tp->relvol / tp->basevol,tp->relvol);
            jaddnum(item,"price",price);
            jaddnum(item,"volume",basevol);
        }
    }
    return(item);
}

cJSON *LP_tradebot_json(struct LP_tradebot *bot)
{
    int32_t i; double aveprice,basevolume,vol; cJSON *json,*array;
    json = cJSON_CreateObject();
    jaddstr(json,"result","success");
    jaddstr(json,"name",bot->name);
    jaddnum(json,"botid",bot->id);
    jaddnum(json,"started",bot->started);
    if ( bot->pause != 0 )
        jaddnum(json,"paused",bot->pause);
    if ( bot->dead != 0 )
        jaddnum(json,"stopped",bot->dead);
    if ( bot->dispdir > 0 )
    {
        jaddstr(json,"action","buy");
        jaddstr(json,"base",bot->base);
        jaddstr(json,"rel",bot->rel);
        jaddnum(json,"maxprice",bot->maxprice);
        jaddnum(json,"totalrelvolume",bot->totalrelvolume);
        jaddnum(json,"totalbasevolume",bot->totalbasevolume);
        if ( (vol= bot->relsum) > SMALLVAL && bot->basesum > SMALLVAL )
        {
            jaddnum(json,"aveprice",vol/bot->basesum);
            jaddnum(json,"volume",vol);
        }
    }
    else
    {
        jaddstr(json,"action","sell");
        jaddstr(json,"base",bot->rel);
        jaddstr(json,"rel",bot->base);
        aveprice = LP_pricevol_invert(&basevolume,bot->maxprice,bot->totalrelvolume);
        jaddnum(json,"minprice",aveprice);
        jaddnum(json,"totalbasevolume",bot->totalrelvolume);
        jaddnum(json,"totalrelvolume",basevolume);
        if ( (vol= bot->relsum) > SMALLVAL && bot->basesum > SMALLVAL )
        {
            aveprice = LP_pricevol_invert(&basevolume,vol/bot->basesum,vol);
            jaddnum(json,"aveprice",aveprice);
            jaddnum(json,"volume",basevolume);
        }
    }
    array = cJSON_CreateArray();
    LP_tradebot_calcstats(bot);
    for (i=0; i<bot->numtrades; i++)
        jaddi(array,LP_tradebot_tradejson(bot->trades[i],bot->dispdir));
    jadd(json,"trades",array);
    if ( bot->basesum > SMALLVAL && bot->relsum > SMALLVAL && bot->completed > 0 )
    {
        jaddnum(json,"completed",bot->completed);
        jaddnum(json,"percentage",100. * (bot->relsum / bot->totalrelvolume));
        if ( bot->dispdir > 0 )
        {
            jaddnum(json,"aveprice",bot->relsum / bot->basesum);
            jaddnum(json,"volume",bot->relsum);
        }
        else
        {
            jaddnum(json,"aveprice",bot->basesum / bot->relsum);
            jaddnum(json,"volume",bot->basesum);
        }
    }
    if ( bot->pendbasesum > SMALLVAL && bot->pendrelsum > SMALLVAL && bot->numpending > 0 )
    {
        jaddnum(json,"pending",bot->numpending);
        if ( bot->dispdir > 0 )
        {
            jaddnum(json,"pendingprice",bot->pendrelsum / bot->pendbasesum);
            jaddnum(json,"pendingvolume",bot->pendrelsum);
        }
        else
        {
            jaddnum(json,"pendingprice",bot->pendbasesum / bot->pendrelsum);
            jaddnum(json,"pendingvolume",bot->pendbasesum);
        }
    }
    return(json);
}

struct LP_tradebot *_LP_tradebotfind(uint32_t botid)
{
    struct LP_tradebot *tmp,*bot,*retbot = 0;
    DL_FOREACH_SAFE(LP_tradebots,bot,tmp)
    {
        if ( botid == bot->id )
        {
            retbot = bot;
            break;
        }
    }
    return(retbot);
}

struct LP_tradebot *LP_tradebotfind(uint32_t botid)
{
    struct LP_tradebot *retbot = 0;
    portable_mutex_lock(&LP_tradebotsmutex);
    retbot = _LP_tradebotfind(botid);
    portable_mutex_unlock(&LP_tradebotsmutex);
    return(retbot);
}

void LP_tradebotadd(struct LP_tradebot *bot)
{
    portable_mutex_lock(&LP_tradebotsmutex);
    while ( _LP_tradebotfind(bot->id) != 0 )
    {
        printf("BOT collision at %u, ok if rare\n",bot->id);
        bot->id++;
    }
    DL_APPEND(LP_tradebots,bot);
    portable_mutex_unlock(&LP_tradebotsmutex);
}

struct LP_tradebot_trade *LP_tradebot_pending(struct LP_tradebot *bot,cJSON *pending,uint32_t tradeid)
{
    struct LP_tradebot_trade *tp;
    tp = calloc(1,sizeof(*tp));
    tp->tradeid = tradeid;
    tp->maxprice = bot->maxprice;
    tp->totalrelvolume = bot->totalrelvolume;
    tp->started = (uint32_t)time(NULL);
    tp->dispdir = bot->dispdir;
    strcpy(tp->base,bot->base);
    strcpy(tp->rel,bot->rel);
    tp->aliceid = j64bits(pending,"aliceid");
    tp->basevol = jdouble(pending,"basevalue");
    tp->relvol = jdouble(pending,"relvalue");
    printf("tradebot pending basevol %.8f relvol %.8f\n",tp->basevol,tp->relvol);
    bot->pendrelsum += tp->relvol;
    bot->pendbasesum += tp->basevol;
    bot->numpending++;
    return(tp);
}

void LP_tradebot_timeslice(void *ctx,struct LP_tradebot *bot)
{
    double remaining; int32_t i,maxiters = 10; uint32_t tradeid; bits256 destpubkey; char *retstr,*liststr; cJSON *retjson,*retjson2,*pending;
    memset(destpubkey.bytes,0,sizeof(destpubkey));
    if ( bot->dead == 0 && bot->pause == 0 && bot->numtrades < sizeof(bot->trades)/sizeof(*bot->trades) )
    {
        if ( (liststr= LP_recent_swaps(0)) != 0 )
        {
            if ( (retjson= cJSON_Parse(liststr)) != 0 )
            {
                if ( jobj(retjson,"pending") == 0 )
                {
                    remaining = bot->totalrelvolume - (bot->relsum + bot->pendrelsum);
                    printf("try autobuy %s/%s remaining %.8f maxprice %.8f\n",bot->base,bot->rel,remaining,bot->maxprice);
                    tradeid = rand();
                    for (i=1; i<=maxiters; i++)
                    {
                        if ( (retstr= LP_autobuy(ctx,LP_myipaddr,LP_mypubsock,bot->base,bot->rel,bot->maxprice,remaining/i,0,0,G.gui,0,destpubkey,tradeid)) != 0 )
                        {
                            if ( (retjson2= cJSON_Parse(retstr)) != 0 )
                            {
                                if ( (pending= jobj(retjson2,"pending")) != 0 && juint(pending,"tradeid") == tradeid )
                                {
                                    bot->trades[bot->numtrades++] = LP_tradebot_pending(bot,pending,tradeid);
                                    if ( bot->relsum >= 0.99*bot->totalrelvolume-SMALLVAL || bot->basesum >= 0.99*bot->totalbasevolume-SMALLVAL )
                                        bot->dead = (uint32_t)time(NULL);
                                    else if ( (bot->pendrelsum+bot->relsum) >= 0.99*bot->totalrelvolume-SMALLVAL || (bot->basesum+bot->pendbasesum) >= 0.99*bot->totalbasevolume-SMALLVAL )
                                        bot->pause = (uint32_t)time(NULL);
                                    printf("issued bot trade.%u %s\n",tradeid,retstr);
                                    free_json(retjson2);
                                    free(retstr);
                                    break;
                                } else printf("iter.%d/%d %.8f didnt get any trade pending %s %s\n\n",i,maxiters,remaining/i,bot->name,retstr);
                                free_json(retjson2);
                            } else printf("iter.%d/%d %.8f %s\n",i,maxiters,remaining/i,retstr);
                            free(retstr);
                        }
                    }
                }
                free_json(retjson);
            }
            free(liststr);
        }
    }
    else if ( bot->pause == 0 )
        bot->pause = (uint32_t)time(NULL);
}

void LP_tradebot_finished(uint32_t tradeid,uint32_t requestid,uint32_t quoteid)
{
    struct LP_tradebot *bot,*tmp; int32_t i; struct LP_tradebot_trade *tp;
    DL_FOREACH_SAFE(LP_tradebots,bot,tmp)
    {
        for (i=0; i<bot->numtrades; i++)
        {
            if ( (tp= bot->trades[i]) != 0 && tp->finished == 0 && tp->tradeid == tradeid )
            {
                tp->requestid = requestid;
                tp->quoteid = quoteid;
                bot->pendbasesum -= tp->basevol, bot->basesum += tp->basevol;
                bot->pendrelsum -= tp->relvol, bot->relsum += tp->relvol;
                bot->numpending--, bot->completed++;
                printf("bot.%u detected completion tradeid.%u aliceid.%llx r.%u q.%u, numpending.%d completed.%d\n",bot->id,tp->tradeid,(long long)tp->aliceid,tp->requestid,tp->quoteid,bot->numpending,bot->completed);
                tp->finished = (uint32_t)time(NULL);
                break;
            }
        }
    }
}

void LP_tradebots_timeslice(void *ctx)
{
    static uint32_t lastnumfinished = 0;
    struct LP_tradebot_trade *tp; struct iguana_info *relcoin; struct LP_tradebot *bot,*tmp; int32_t i;
    DL_FOREACH_SAFE(LP_tradebots,bot,tmp)
    {
        if ( (relcoin= LP_coinfind(bot->rel)) != 0 )
            LP_listunspent_issue(bot->rel,relcoin->smartaddr,1);
        if ( bot->numpending > 0 && LP_numfinished > lastnumfinished )
        {
            // expire pending trades and see if any still need their requestid/quoteid
            for (i=0; i<bot->numtrades; i++)
            {
                if ( (tp= bot->trades[i]) != 0 && tp->finished == 0 )
                {
                    if ( time(NULL) > tp->started+INSTANTDEX_LOCKTIME*2 )
                    {
                        bot->pendbasesum -= tp->basevol;
                        bot->pendrelsum -= tp->relvol;
                        bot->numpending--;
                        tp->finished = (uint32_t)time(NULL);
                        printf("%s trade.%d of %d expired\n",bot->name,i,bot->numtrades);
                    }
                }
            }
        }
        else //if ( bot->numpending == 0 )
            LP_tradebot_timeslice(ctx,bot);
    }
    lastnumfinished = LP_numfinished;
}

char *LP_tradebot_list(void *ctx,int32_t pubsock,cJSON *argjson)
{
    struct LP_tradebot *bot,*tmp; cJSON *array = cJSON_CreateArray();
    DL_FOREACH_SAFE(LP_tradebots,bot,tmp)
    {
        jaddinum(array,bot->id);
    }
    return(jprint(array,1));
}

char *LP_tradebot_buy(int32_t dispdir,char *base,char *rel,double maxprice,double relvolume)
{
    struct LP_tradebot *bot; char *retstr; double shortfall; int32_t i,n; cJSON *array,*item,*retjson; uint64_t txfees,balance=0,abalance=0; struct iguana_info *basecoin,*relcoin;
    basecoin = LP_coinfind(base);
    relcoin = LP_coinfind(rel);
    if ( basecoin == 0 || relcoin == 0 || basecoin->inactive != 0 || relcoin->inactive != 0 )
        return(clonestr("{\"error\":\"one or more coins inactive\"}"));
    if ( (array= LP_inventory(rel)) != 0 )
    {
        if ( (n= cJSON_GetArraySize(array)) > 0 && is_cJSON_Array(array) != 0 )
        {
            for (i=0; i<n; i++)
            {
                item = jitem(array,i);
                abalance += j64bits(item,"satoshis");
            }
        }
        free_json(array);
    }
    if ( (retstr= LP_orderbook(base,rel,0)) != 0 )
        free(retstr);
    txfees = 10 * relcoin->txfee;
    printf("%s inventory balance %.8f, relvolume %.8f + txfees %.8f\n",rel,dstr(abalance),relvolume,dstr(txfees));
    if ( dstr(abalance) < relvolume + dstr(txfees) )
    {
        retjson = cJSON_CreateObject();
        jaddstr(retjson,"error","not enough funds");
        jaddstr(retjson,"coin",rel);
        jaddnum(retjson,"balance",dstr(abalance));
        jaddnum(retjson,"relvolume",relvolume);
        jaddnum(retjson,"txfees",dstr(txfees));
        shortfall = (relvolume + dstr(txfees)) - dstr(balance);
        jaddnum(retjson,"shortfall",shortfall);
        if ( (balance= LP_RTsmartbalance(relcoin)) > abalance+SATOSHIDEN*(shortfall+relvolume/77.) )
        {
            char *withdrawstr; cJSON *outputjson,*withdrawjson,*outputs,*item;
            outputjson = cJSON_CreateObject();
            outputs = cJSON_CreateArray();
            item = cJSON_CreateObject();
            jaddnum(item,relcoin->smartaddr,relvolume+dstr(txfees));
            jaddi(outputs,item);
            item = cJSON_CreateObject();
            jaddnum(item,relcoin->smartaddr,(relvolume+dstr(txfees))/777);
            jaddi(outputs,item);
            item = cJSON_CreateObject();
            jaddnum(item,relcoin->smartaddr,(relvolume+dstr(txfees))/777);
            jaddi(outputs,item);
            item = cJSON_CreateObject();
            jaddnum(item,relcoin->smartaddr,(relvolume+dstr(txfees))/777);
            jaddi(outputs,item);
            jadd(outputjson,"outputs",outputs);
            if ( (withdrawstr= LP_withdraw(relcoin,outputjson)) != 0 )
            {
                if ( (withdrawjson= cJSON_Parse(withdrawstr)) != 0 )
                    jadd(retjson,"withdraw",withdrawjson);
                free(withdrawstr);
            }
            free_json(outputjson);
        }
        return(jprint(retjson,1));
    }
    printf("disp.%d tradebot_buy(%s / %s) maxprice %.8f relvolume %.8f\n",dispdir,base,rel,maxprice,relvolume);
    if ( (bot= calloc(1,sizeof(*bot))) != 0 )
    {
        safecopy(bot->base,base,sizeof(bot->base));
        safecopy(bot->rel,rel,sizeof(bot->rel));
        bot->dispdir = dispdir;
        bot->maxprice = maxprice;
        bot->totalrelvolume = relvolume;
        LP_pricevol_invert(&bot->totalbasevolume,maxprice,relvolume);
        bot->started = (uint32_t)time(NULL);
        if ( dispdir > 0 )
            sprintf(bot->name,"buy_%s_%s.%d",base,rel,bot->started);
        else sprintf(bot->name,"sell_%s_%s.%d",rel,base,bot->started);
        bot->id = calc_crc32(0,(uint8_t *)bot,sizeof(*bot));
        LP_tradebotadd(bot);
        return(jprint(LP_tradebot_json(bot),1));
    }
    return(0);
}

char *LP_tradebot_limitbuy(void *ctx,int32_t pubsock,cJSON *argjson)
{
    double relvolume,maxprice; char *base,*rel;
    base = jstr(argjson,"base");
    rel = jstr(argjson,"rel");
    maxprice = jdouble(argjson,"maxprice");
    relvolume = jdouble(argjson,"relvolume");
    printf("limit buy %s/%s %.8f %.8f\n",base,rel,maxprice,relvolume);
    if ( LP_priceinfofind(base) != 0 && LP_priceinfofind(rel) != 0 && maxprice > SMALLVAL && maxprice < SATOSHIDEN && relvolume > 0.0001 && relvolume < SATOSHIDEN )
        return(LP_tradebot_buy(1,base,rel,maxprice,relvolume));
    return(clonestr("{\"error\":\"invalid parameter\"}"));
}

char *LP_tradebot_limitsell(void *ctx,int32_t pubsock,cJSON *argjson)
{
    double relvolume,maxprice,price,basevolume,p,v; char *base,*rel;
    base = jstr(argjson,"base");
    rel = jstr(argjson,"rel");
    price = jdouble(argjson,"minprice");
    basevolume = jdouble(argjson,"basevolume");
    if ( LP_priceinfofind(base) != 0 && LP_priceinfofind(rel) != 0 && price > SMALLVAL && price < SATOSHIDEN && basevolume > 0.0001 && basevolume < SATOSHIDEN )
    {
        maxprice = price;
        relvolume = (price * basevolume);
        p = LP_pricevol_invert(&v,maxprice,relvolume);
        printf("minprice %.8f basevolume %.8f -> (%.8f %.8f) -> (%.8f %.8f)\n",price,basevolume,maxprice,relvolume,1./p,v);
        return(LP_tradebot_buy(-1,rel,base,p,v));
    }
    return(clonestr("{\"error\":\"invalid parameter\"}"));
}

char *LP_tradebot_settings(void *ctx,int32_t pubsock,cJSON *argjson,uint32_t botid)
{
    struct LP_tradebot *bot; double newprice,newvolume;
    if ( (bot= LP_tradebotfind(botid)) != 0 )
    {
        if ( bot->dead != 0 )
            return(clonestr("{\"error\":\"botid aleady stopped\"}"));
        newprice = jdouble(argjson,"newprice");
        newvolume = jdouble(argjson,"newvolume");
        if ( (newprice > SMALLVAL && newprice < SATOSHIDEN) || (newvolume > 0.0001 && newvolume < SATOSHIDEN) )
        {
            if ( bot->dispdir < 0 )
            {
                if ( newprice > SMALLVAL )
                    bot->maxprice = 1. / newprice;
                if ( newvolume > SMALLVAL )
                    bot->totalrelvolume = (bot->maxprice * newvolume);
            }
            else
            {
                if ( newprice > SMALLVAL )
                    bot->maxprice = newprice;
                if ( newvolume > SMALLVAL )
                    bot->totalrelvolume = newvolume;
            }
        }
        return(jprint(LP_tradebot_json(bot),1));
    }
    return(clonestr("{\"error\":\"couldnt find botid\"}"));
}

char *LP_tradebot_status(void *ctx,int32_t pubsock,cJSON *argjson,uint32_t botid)
{
    struct LP_tradebot *bot;
    if ( (bot= LP_tradebotfind(botid)) != 0 )
        return(jprint(LP_tradebot_json(bot),1));
    return(clonestr("{\"error\":\"couldnt find botid\"}"));
}

char *LP_tradebot_stop(void *ctx,int32_t pubsock,cJSON *argjson,uint32_t botid)
{
    struct LP_tradebot *bot;
    if ( (bot= LP_tradebotfind(botid)) != 0 )
    {
        bot->dead = (uint32_t)time(NULL);
        return(clonestr("{\"result\":\"success\"}"));
    }
    return(clonestr("{\"error\":\"couldnt find botid\"}"));
}

char *LP_tradebot_pause(void *ctx,int32_t pubsock,cJSON *argjson,uint32_t botid)
{
    struct LP_tradebot *bot;
    if ( (bot= LP_tradebotfind(botid)) != 0 )
    {
        if ( bot->dead != 0 )
            return(clonestr("{\"error\":\"botid aleady stopped\"}"));
        bot->pause = (uint32_t)time(NULL);
        return(clonestr("{\"result\":\"success\"}"));
    }
    return(clonestr("{\"error\":\"couldnt find botid\"}"));
}

char *LP_tradebot_resume(void *ctx,int32_t pubsock,cJSON *argjson,uint32_t botid)
{
    struct LP_tradebot *bot;
    if ( (bot= LP_tradebotfind(botid)) != 0 )
    {
        if ( bot->dead != 0 )
            return(clonestr("{\"error\":\"botid aleady stopped\"}"));
        if ( bot->pause == 0 )
            return(clonestr("{\"result\":\"success\",\"status\":\"botid not paused\"}"));
        bot->pause = 0;
        return(clonestr("{\"result\":\"success\"}"));
    }
    return(clonestr("{\"error\":\"couldnt find botid\"}"));
}

char *LP_istradebots_command(void *ctx,int32_t pubsock,char *method,cJSON *argjson)
{
    uint32_t botid;
    if ( strncmp("bot_",method,strlen("bot_")) != 0 )
        return(0);
    if ( strcmp(method,"bot_list") == 0 )
        return(LP_tradebot_list(ctx,pubsock,argjson));
    else if ( strcmp(method,"bot_buy") == 0 )
        return(LP_tradebot_limitbuy(ctx,pubsock,argjson));
    else if ( strcmp(method,"bot_sell") == 0 )
        return(LP_tradebot_limitsell(ctx,pubsock,argjson));
    if ( (botid= juint(argjson,"botid")) == 0 )
        return(clonestr("{\"error\":\"no botid specified\"}"));
    else
    {
        if ( strcmp(method,"bot_status") == 0 )
            return(LP_tradebot_status(ctx,pubsock,argjson,botid));
        else if ( strcmp(method,"bot_settings") == 0 )
            return(LP_tradebot_settings(ctx,pubsock,argjson,botid));
        else if ( strcmp(method,"bot_stop") == 0 )
            return(LP_tradebot_stop(ctx,pubsock,argjson,botid));
        else if ( strcmp(method,"bot_pause") == 0 )
            return(LP_tradebot_pause(ctx,pubsock,argjson,botid));
        else if ( strcmp(method,"bot_resume") == 0 )
            return(LP_tradebot_resume(ctx,pubsock,argjson,botid));
    }
    return(0);
}

