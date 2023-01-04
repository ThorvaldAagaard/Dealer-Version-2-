// dealerv2.c  version 2.0.x by JGM 2022-Feb-15
// Version 2.0.1  remove MSC stuff ... & Francois Stuff ... & GIB Library file
//             2.0.2  Add LTC, Title, Printside, and other minor enhancements
//             2.0.3  Add DDS calculation of Tricks and Par.
//             2.0.4  Added -D switch on cmd line. Testing why DDS is so slow.
//             2.0.5  Complete redo of DDS to use binary not PBN deals. restructured dirs. added more cmd line switches
//             2.0.6  Introduced code to run DOP perl pgm and save results in dealer struct
//             2.0.7  Export code added. -X option processing.
//             2.0.8  CSVRPT code added. -C option processing.
//             2.0.9  seed in input file code added.
//             2.1.0  Francois Dellacherie shapes added to Dealer input file. Use external program to expand
// 2022/09/19  2.2.0  Fixed some bugs in redefining the altcount and pointcount arrays. and in use of dotnums.
// 2022/10/05  2.3.0  Added Bucket Frequency functionality
// 2023/01/03  2.3.1  Fixed bug Predeal / StackedPack handling by sorting stacked_pack.
//
#ifndef _GNU_SOURCE
  #define _GNU_SOURCE
#endif
#include "../include/std_hdrs.h"  /* all the stdio.h stdlib.h string.h assert.h etc. */

#include "../include/dealdefs.h"
#include "../include/dealtypes.h"
#include "../include/dealprotos.h"
#include "../include/dealexterns.h"   /* was dealglobals but testing if I can compile globals separately and link it. */
#include "../include/pointcount.h"
#include "../src/UsageMsg.h"

/* next file for some subs that main uses to setup runtime and debug statements */
#include "mainsubs.c"

/* This next one is in case we want to use 64 bit ints to hold the distribution bits.
 * To allow for 64 shape statements instead of 'only' 32 shape statements.
 * then we can define MAXDISTR as 8*sizeof(long int) or thereabouts
 */
#ifndef MAXDISTR
 #define MAXDISTR 8*sizeof(int)
#endif

void hexdeal_show( deal dx );
void sr_deal_show( deal dx );

int main (int argc, char **argv) {
#ifdef JGMDBG
    jgmDebug = 1 ; // Assume we want some debug output. User can always set to zero from cmd line ..
#endif
    extern int yyparse  (void) ;
    void  SetResources(  int maxMemoryMB,  int maxThreads); /* for DDS */
    /* opts collects all cmd line parms in one place. Vars from previous versions of Dealer kept also so some duplication */
    struct options_st *opts ;
    struct param_st   *scr_varp ; /* pointer to script variables struct */
    int i ;
    int keephand ;

    scr_varp = &parm ;  /* parm is a global */
    memset( &parm, 0 , sizeof(parm) ) ;   /* filling with zeros should both terminate the 'strings' and set len to 0 */

    opts = &options ;         /* struct should have been init to all zeroes or null strings in dealglobals.c */
    opts->par_vuln =   -1 ;   /* default no Par calcs            -P switch        */
    opts->dds_mode =    1 ;   /* solve one board                 -M switch        */
    opts->nThreads =    9 ;   /* The sweet spot; >9 no effect    -R switch        */
    opts->maxRamMB = 1440 ;   /* 160 MB per thread is the max we need for DDS     */
    opts->verbose  = verbose; /* some opt globals are set to 1 and then toggled. keep in sync */
    opts->progress_meter = progressmeter ; /* also a toggle */
    opts->upper_case = uppercase;          /* also a toggle */
    opts->opc_opener = opc_opener ;        /* -O switch; minor impact on how OPC calcs its pts */
    opts->opener = Opener ;
    opts->accross_bkts = 42 ;       /* -a switch for 2D freq plots. 42 allows for 0-40 hcp plus lo and hi. */
    opts->down_bkts    = 42 ;       /* -d switch for 1D and 2D freq plots. */
                                    /* max bkts accross and down needed because dotnums can vary from 0 to 4000 say. */
    memset(opts->preDeal_len, 0, 4*sizeof(int) ) ; /* preDeal stores the -N,-E,-S,-W opt settings */
    memset(opts->preDeal, '\0' , 128 ) ;

    /* The non opt versions of the above will have been compile time init in dealglobals.c */
    /* None of the following are set in the input file and so should be replaced by the options.xxxx variable */
    errflg = 0;
    progressmeter = 0;
    swapping = 0;
    swapindex = 0;
    seed_provided = 0 ;
    verbose = 1;
    quiet = 0 ;
    uppercase = 0 ;
    input_file = '\0' ;

  struct timeval tvstart, tvstop;
  gettimeofday (&tvstart, (void *) 0);
  fexp = stdout ; // FILE *fexp Will be opened by getopts if there is a -X option
  fcsv = stdout ; // FILE *fcsv Will be opened by getopts if there is a -C option

 /* process cmd line options. Save them for later after parsing done */
 /* Note that get_options returns immediately after it has found a -V or -h switch
  * so put any other options such as -D or -T that you want to affect the display, BEFORE
  * the said switch. e.g. dealerv2 -D3 -T "Test Run" -M2 -C w:/tmp/MyDataFile.csv -0 12 - 1 14 -V
  * This will show the options struct contents and the script vars before the version info and exiting.
  */
  get_options( argc, argv, opts) ;

   #ifdef JGMDBG
        if(jgmDebug > 0 ) {  fprintf(stderr, "JGMDBG DEFINED= %d in main \n",JGMDBG ); }
        if(jgmDebug >= 2) {  fprintf(stderr, "--------HELLO FROM VERSION %s ---------- \n", VERSION); }
        #ifdef YYDEBUG
             if(jgmDebug > 0 ) {  fprintf(stderr, "YYDEBUG DEFINED yydebug== %d BISON DBG Active.  \n",yydebug ); }
        #else
            if(jgmDebug > 0 ) {  fprintf(stderr, "YYDEBUG NOT Defined.  BISON DBG IN_Active.  \n" ); }
        #endif
    #endif

  extern void show_options     ( struct options_st *opts , int v ) ;
  extern void show_script_vars ( struct param_st   *p    , int v ) ;

  /* Even if compiled without JGMDBG defined, we still give user some minimal Debug info if he asks for it */
  if(jgmDebug > 0) {  show_options(opts, 1) ; }
  if(jgmDebug > 0) {  show_script_vars( scr_varp, 1 ) ; }

  if (3 == options.options_error || 4 == options.options_error  ) { /* Print Version Info(3) or Help/Usage(4) and exit */
      return 0 ;
  }

  if (argc - optind > 2 || errflg) {
    fprintf (stderr, "Usage: %s %s\n", argv[0], UsageMessage );
    exit (-1);
  }
  if (optind < argc && freopen (input_file = argv[optind], "r", stdin) == NULL) {
    perror (argv[optind]);
    exit (-1);
  }
   newpack(fullpack);   /* fill fullpack with cards in order from Club Deuce(North) up to Spade Ace(West) */

  for (i = 0; i < 52; i++) stacked_pack[i] = NO_CARD; /* stacked pack used for preDeals */
  initdistr ();
  maxdealer = -1;  /* set so that yyparse/flex will fill them in if reqd */
  maxvuln = -1;
  dds_dealnum = -1;
#ifdef JGMDBG
     if (jgmDebug >= 3) { fprintf(stderr, "Calling yyparse from file[%s] at line [%d]\n",__FILE__,__LINE__); }
#endif

/* ===============> YYPARSE HERE <=============== */
  yyparse ();   /* build the list of conditions to evaluate and the list of actions to do */
  /* make sure that the points array (of A, K, Q, J, etc.) is in sync with the tblPointcount [idxHCP] array.
   * The latter may have been redefined by the user via a pointcount statement
   */
   for (i=0 ; i< 13 ; i++ ) { points[i] = tblPointcount[idxHcp][i] ; }  /* V4.5 */

  /* ----------------- Parsing of User Specs done. Decision Tree built. Action list built. -------------------*/
  /*                     Proceed to generate random hands, evaluate them, and print them                      */
#ifdef JGMDBG
  if (jgmDebug >= 3) {
    fprintf(stderr, " After yyparse:: maxproduce=%d, maxgenerate=%d, maxdealer=%d, maxvuln=%d, Opener=%c, Title=%s\n",
                     maxproduce,    maxgenerate,    maxdealer,      maxvuln,   opc_opener, title );
    fprintf(stderr, "Showing yyparse generated Decision tree, varlist, actionlist  \n");
    fprintf(stderr, "Decision Tree starts at %p with type %d \n", (void *)decisiontree, decisiontree->tr_type);
    /* Expression List is only used by the printes action. Prob Not necessary to show it. */
    showdecisiontree(decisiontree);
    showvarlist(vars);  fprintf(stderr, "\nVARLIST DONE \n");
    showactionlist(actionlist); fprintf(stderr, "\nACTION List DONE \n");
    if (jgmDebug >= 4) {
        showdistrbits(distrbitmaps) ; fprintf(stderr, "\nDistr Bit Maps DONE \n");
        showAltCounts() ; fprintf (stderr, "Show ALT Counts Done \n");
    }
    fprintf(stderr, "\n-----------------------\n");
  } /* end if jgmDebug */
#endif
  /* Vers 2.0.9 -- the seed could come from input file or from cmd line -- move initprogram to before RNG init */
  /* Vers 2.1.0 -- Added Francois shapes to Descr file. Expanded by his Perl program */

  initprogram(opts);  /* here we will over-ride yyparse if reqd from cmd line and do other stuff. */

  /*Using glibc standard lib routines rand48 and srand48.*/
  /* If no seed provided, use function init_rand48 which uses kernel as seed source*/
  if (jgmDebug >= 3 ) { DBGPRT("Initializing RNG. Seed_providedx10000 + seed",(seed_provided*10000+seed)," in main "); }
  if (!seed_provided) {
    seed = SRANDOM(0) ; /* use init_rand48() to init the RNG with a seed that the kernel provides */
  }
  else {
    seed = SRANDOM(seed) ; /* use init_rand48() to init the RNG with the users seed */
  }
  if (jgmDebug >= 3 ) { DBGPRT("Seed val=",seed,"Calling Init prog now"); }
  //SetMaxThreads(9);
  SetResources(opts->maxRamMB, opts->nThreads) ; /* 160MB/Thread max; 9,12,16 threads all equal and 25% faster than 6 Threads*/

  if (maxgenerate == 0) maxgenerate = 10000000;
  if (maxproduce == 0)  maxproduce = ((actionlist == &defaultaction) || will_print) ? 40 : maxgenerate;
#ifdef JGMDBG
  if (jgmDebug >= 2) { fprintf(stderr, "Maxgenerate=%d, Maxproduce=%d, Calling setup_action \n", maxgenerate, maxproduce ); }
#endif
  setup_action();

  if (progressmeter) fprintf (stderr, "Calculating...  0%% complete\r"); // \r CR not \n since want same line ..
  setup_deal(); /* fill curdeal taking into account the Predeal and swapping requirements in stacked pack*/

  /* ----------- Begin the Main Loop ------------*/
  for (ngen = 1, nprod = 0; ngen <= maxgenerate && nprod < maxproduce; ngen++) { /* start ngen at 1; simplifies counting */
      treedepth = 0;
      if (jgmDebug >= 8 )
            DBGPRT("In Main Generating Hands: ngen=",ngen," Calling Shuffle");
      shuffle (curdeal);
      #ifdef JGMDBG
        if (jgmDebug >= 9) {  sr_deal_show(curdeal); }
      #endif
          analyze (curdeal, hs);  // Collect and save all info that will be needed by eval_tree() aka interesting() */
   #ifdef JGMDBG
      if (jgmDebug >= 8 ) { fprintf(stderr, "Main.164:: Calling Interesting for ngen=%d \n", ngen);  }
   #endif
      keephand = interesting() ;  /* will showtree if in DBG mode */
      showtree = 0 ;              /*in debug mode only need to show the tree once; its the same for all hands */
      if (keephand) {             /* evaltree returns TRUE for the condition user specified */
           if (jgmDebug >= 8 ) { DBGPRT("Interesting returns true at line ", __LINE__ , "Calling Action() now."); }
          action();                        /* Do action list */
          nprod++;
          if (progressmeter) {
            if ((100 * nprod / maxproduce) > 100 * (nprod - 1) / maxproduce)
              fprintf (stderr, "Calculating... %2d%% complete\r",
               100 * nprod / maxproduce);
          } /* end progress meter */
      }   /* end keephand */
      #ifdef JGMDBG
        else { if ( jgmDebug >= 9) { fprintf(stderr, "Generated Hand %d is NOT interesting .. skipping actions \n",ngen) ; }
        }
      #endif
   }  /* end for ngen */
   if ( ngen >= maxgenerate ) { ngen = maxgenerate ; } /* need this for end of run stats ... for loop leaves it at max+1 or one over */
   else if (ngen >= 2 ) {ngen-- ; }
    /* finished generating deals, either maxgenerate or maxproduce was reached */
  if (progressmeter) fprintf (stderr, "                                      \r");
  gettimeofday (&tvstop, (void *) 0);

  cleanup_action();                /* This will do all the end of run actions such as: Average, Frequency, Frequency2D */

  if (verbose) {
    if (strlen(title) > 0 ) { printf("\n%s\n",title); }
    printf ("Generated %d hands\n", ngen);
    printf ("Produced  %d hands\n", nprod);
    printf ("Initial random seed %lu\n", seed);
    printf ("Time needed %8.3f sec%s",
             (tvstop.tv_sec + tvstop.tv_usec / 1000000.0 -
             (tvstart.tv_sec + tvstart.tv_usec / 1000000.0)), crlf);
    if (jgmDebug >= 2 ) { /* print some additional info for user even if JGMDBG Code not included */
      fprintf(stdout, "Tot Calls to DDS =%6d, DDS Solve Calls=%6d\n", dbg_dds_res_calls,  dbg_dds_lib_calls );
      fprintf(stdout, "Tot Calls to GIB =%6d, GIB Solve Calls=%6d\n", dbg_dd_calls, dbg_tdd_calls );
      fprintf(stdout, "Tot Calls to OPC =%6d, OPC Calc  Calls=%6d\n", dbg_opc_calls, dbg_opc_cmd_calls );

    } /* end if jgmDebug */
  } /* end if verbose */

  return 0;
} /* end main */


