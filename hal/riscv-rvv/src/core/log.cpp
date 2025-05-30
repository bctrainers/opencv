// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level
// directory of this distribution and at http://opencv.org/license.html.

// Copyright (C) 2025, Institute of Software, Chinese Academy of Sciences.

#include "rvv_hal.hpp"

namespace cv { namespace rvv_hal { namespace core {

#if CV_HAL_RVV_1P0_ENABLED

namespace detail {

static constexpr size_t log_scale = 8;
static constexpr size_t log_tab_size = ((size_t)1 << log_scale) * 2;
static constexpr size_t log_mask = log_tab_size - 2;

static constexpr double ln_2 = 0.69314718055994530941723212145818;

static constexpr size_t log32f_mask = ((size_t)1 << (23 - log_scale)) - 1;
static constexpr double log32f_a0 = 0.3333333333333333333333333f;
static constexpr double log32f_a1 = -0.5f;
static constexpr double log32f_a2 = 1.f;

static constexpr size_t log64f_mask = ((size_t)1 << (52 - log_scale)) - 1;
static constexpr double log64f_a0 = -0.125;
static constexpr double log64f_a1 = 0.1428571428571428769682682968777953647077083587646484375;
static constexpr double log64f_a2 = -0.1666666666666666574148081281236954964697360992431640625;
static constexpr double log64f_a3 = 0.2;
static constexpr double log64f_a4 = -0.25;
static constexpr double log64f_a5 = 0.333333333333333314829616256247390992939472198486328125;
static constexpr double log64f_a6 = -0.5;
static constexpr double log64f_a7 = 1.0;

#define LOG_TAB_VALUE                                                                          \
    {                                                                                          \
        0.0000000000000000000000000000000000000000, 1.000000000000000000000000000000000000000, \
        .00389864041565732288852075271279318258166, .9961089494163424124513618677042801556420, \
        .00778214044205494809292034119607706088573, .9922480620155038759689922480620155038760, \
        .01165061721997527263705585198749759001657, .9884169884169884169884169884169884169884, \
        .01550418653596525274396267235488267033361, .9846153846153846153846153846153846153846, \
        .01934296284313093139406447562578250654042, .9808429118773946360153256704980842911877, \
        .02316705928153437593630670221500622574241, .9770992366412213740458015267175572519084, \
        .02697658769820207233514075539915211265906, .9733840304182509505703422053231939163498, \
        .03077165866675368732785500469617545604706, .9696969696969696969696969696969696969697, \
        .03455238150665972812758397481047722976656, .9660377358490566037735849056603773584906, \
        .03831886430213659461285757856785494368522, .9624060150375939849624060150375939849624, \
        .04207121392068705056921373852674150839447, .9588014981273408239700374531835205992509, \
        .04580953603129420126371940114040626212953, .9552238805970149253731343283582089552239, \
        .04953393512227662748292900118940451648088, .9516728624535315985130111524163568773234, \
        .05324451451881227759255210685296333394944, .9481481481481481481481481481481481481481, \
        .05694137640013842427411105973078520037234, .9446494464944649446494464944649446494465, \
        .06062462181643483993820353816772694699466, .9411764705882352941176470588235294117647, \
        .06429435070539725460836422143984236754475, .9377289377289377289377289377289377289377, \
        .06795066190850773679699159401934593915938, .9343065693430656934306569343065693430657, \
        .07159365318700880442825962290953611955044, .9309090909090909090909090909090909090909, \
        .07522342123758751775142172846244648098944, .9275362318840579710144927536231884057971, \
        .07884006170777602129362549021607264876369, .9241877256317689530685920577617328519856, \
        .08244366921107458556772229485432035289706, .9208633093525179856115107913669064748201, \
        .08603433734180314373940490213499288074675, .9175627240143369175627240143369175627240, \
        .08961215868968712416897659522874164395031, .9142857142857142857142857142857142857143, \
        .09317722485418328259854092721070628613231, .9110320284697508896797153024911032028470, \
        .09672962645855109897752299730200320482256, .9078014184397163120567375886524822695035, \
        .10026945316367513738597949668474029749630, .9045936395759717314487632508833922261484, \
        .10379679368164355934833764649738441221420, .9014084507042253521126760563380281690141, \
        .10731173578908805021914218968959175981580, .8982456140350877192982456140350877192982, \
        .11081436634029011301105782649756292812530, .8951048951048951048951048951048951048951, \
        .11430477128005862852422325204315711744130, .8919860627177700348432055749128919860627, \
        .11778303565638344185817487641543266363440, .8888888888888888888888888888888888888889, \
        .12124924363286967987640707633545389398930, .8858131487889273356401384083044982698962, \
        .12470347850095722663787967121606925502420, .8827586206896551724137931034482758620690, \
        .12814582269193003360996385708858724683530, .8797250859106529209621993127147766323024, \
        .13157635778871926146571524895989568904040, .8767123287671232876712328767123287671233, \
        .13499516453750481925766280255629681050780, .8737201365187713310580204778156996587031, \
        .13840232285911913123754857224412262439730, .8707482993197278911564625850340136054422, \
        .14179791186025733629172407290752744302150, .8677966101694915254237288135593220338983, \
        .14518200984449788903951628071808954700830, .8648648648648648648648648648648648648649, \
        .14855469432313711530824207329715136438610, .8619528619528619528619528619528619528620, \
        .15191604202584196858794030049466527998450, .8590604026845637583892617449664429530201, \
        .15526612891112392955683674244937719777230, .8561872909698996655518394648829431438127, \
        .15860503017663857283636730244325008243330, .8533333333333333333333333333333333333333, \
        .16193282026931324346641360989451641216880, .8504983388704318936877076411960132890365, \
        .16524957289530714521497145597095368430010, .8476821192052980132450331125827814569536, \
        .16855536102980664403538924034364754334090, .8448844884488448844884488448844884488449, \
        .17185025692665920060697715143760433420540, .8421052631578947368421052631578947368421, \
        .17513433212784912385018287750426679849630, .8393442622950819672131147540983606557377, \
        .17840765747281828179637841458315961062910, .8366013071895424836601307189542483660131, \
        .18167030310763465639212199675966985523700, .8338762214983713355048859934853420195440, \
        .18492233849401198964024217730184318497780, .8311688311688311688311688311688311688312, \
        .18816383241818296356839823602058459073300, .8284789644012944983818770226537216828479, \
        .19139485299962943898322009772527962923050, .8258064516129032258064516129032258064516, \
        .19461546769967164038916962454095482826240, .8231511254019292604501607717041800643087, \
        .19782574332991986754137769821682013571260, .8205128205128205128205128205128205128205, \
        .20102574606059073203390141770796617493040, .8178913738019169329073482428115015974441, \
        .20421554142869088876999228432396193966280, .8152866242038216560509554140127388535032, \
        .20739519434607056602715147164417430758480, .8126984126984126984126984126984126984127, \
        .21056476910734961416338251183333341032260, .8101265822784810126582278481012658227848, \
        .21372432939771812687723695489694364368910, .8075709779179810725552050473186119873817, \
        .21687393830061435506806333251006435602900, .8050314465408805031446540880503144654088, \
        .22001365830528207823135744547471404075630, .8025078369905956112852664576802507836991, \
        .22314355131420973710199007200571941211830, .8000000000000000000000000000000000000000, \
        .22626367865045338145790765338460914790630, .7975077881619937694704049844236760124611, \
        .22937410106484582006380890106811420992010, .7950310559006211180124223602484472049689, \
        .23247487874309405442296849741978803649550, .7925696594427244582043343653250773993808, \
        .23556607131276688371634975283086532726890, .7901234567901234567901234567901234567901, \
        .23864773785017498464178231643018079921600, .7876923076923076923076923076923076923077, \
        .24171993688714515924331749374687206000090, .7852760736196319018404907975460122699387, \
        .24478272641769091566565919038112042471760, .7828746177370030581039755351681957186544, \
        .24783616390458124145723672882013488560910, .7804878048780487804878048780487804878049, \
        .25088030628580937353433455427875742316250, .7781155015197568389057750759878419452888, \
        .25391520998096339667426946107298135757450, .7757575757575757575757575757575757575758, \
        .25694093089750041913887912414793390780680, .7734138972809667673716012084592145015106, \
        .25995752443692604627401010475296061486000, .7710843373493975903614457831325301204819, \
        .26296504550088134477547896494797896593800, .7687687687687687687687687687687687687688, \
        .26596354849713793599974565040611196309330, .7664670658682634730538922155688622754491, \
        .26895308734550393836570947314612567424780, .7641791044776119402985074626865671641791, \
        .27193371548364175804834985683555714786050, .7619047619047619047619047619047619047619, \
        .27490548587279922676529508862586226314300, .7596439169139465875370919881305637982196, \
        .27786845100345625159121709657483734190480, .7573964497041420118343195266272189349112, \
        .28082266290088775395616949026589281857030, .7551622418879056047197640117994100294985, \
        .28376817313064456316240580235898960381750, .7529411764705882352941176470588235294118, \
        .28670503280395426282112225635501090437180, .7507331378299120234604105571847507331378, \
        .28963329258304265634293983566749375313530, .7485380116959064327485380116959064327485, \
        .29255300268637740579436012922087684273730, .7463556851311953352769679300291545189504, \
        .29546421289383584252163927885703742504130, .7441860465116279069767441860465116279070, \
        .29836697255179722709783618483925238251680, .7420289855072463768115942028985507246377, \
        .30126133057816173455023545102449133992200, .7398843930635838150289017341040462427746, \
        .30414733546729666446850615102448500692850, .7377521613832853025936599423631123919308, \
        .30702503529491181888388950937951449304830, .7356321839080459770114942528735632183908, \
        .30989447772286465854207904158101882785550, .7335243553008595988538681948424068767908, \
        .31275571000389684739317885942000430077330, .7314285714285714285714285714285714285714, \
        .31560877898630329552176476681779604405180, .7293447293447293447293447293447293447293, \
        .31845373111853458869546784626436419785030, .7272727272727272727272727272727272727273, \
        .32129061245373424782201254856772720813750, .7252124645892351274787535410764872521246, \
        .32411946865421192853773391107097268104550, .7231638418079096045197740112994350282486, \
        .32694034499585328257253991068864706903700, .7211267605633802816901408450704225352113, \
        .32975328637246797969240219572384376078850, .7191011235955056179775280898876404494382, \
        .33255833730007655635318997155991382896900, .7170868347338935574229691876750700280112, \
        .33535554192113781191153520921943709254280, .7150837988826815642458100558659217877095, \
        .33814494400871636381467055798566434532400, .7130919220055710306406685236768802228412, \
        .34092658697059319283795275623560883104800, .7111111111111111111111111111111111111111, \
        .34370051385331840121395430287520866841080, .7091412742382271468144044321329639889197, \
        .34646676734620857063262633346312213689100, .7071823204419889502762430939226519337017, \
        .34922538978528827602332285096053965389730, .7052341597796143250688705234159779614325, \
        .35197642315717814209818925519357435405250, .7032967032967032967032967032967032967033, \
        .35471990910292899856770532096561510115850, .7013698630136986301369863013698630136986, \
        .35745588892180374385176833129662554711100, .6994535519125683060109289617486338797814, \
        .36018440357500774995358483465679455548530, .6975476839237057220708446866485013623978, \
        .36290549368936841911903457003063522279280, .6956521739130434782608695652173913043478, \
        .36561919956096466943762379742111079394830, .6937669376693766937669376693766937669377, \
        .36832556115870762614150635272380895912650, .6918918918918918918918918918918918918919, \
        .37102461812787262962487488948681857436900, .6900269541778975741239892183288409703504, \
        .37371640979358405898480555151763837784530, .6881720430107526881720430107526881720430, \
        .37640097516425302659470730759494472295050, .6863270777479892761394101876675603217158, \
        .37907835293496944251145919224654790014030, .6844919786096256684491978609625668449198, \
        .38174858149084833769393299007788300514230, .6826666666666666666666666666666666666667, \
        .38441169891033200034513583887019194662580, .6808510638297872340425531914893617021277, \
        .38706774296844825844488013899535872042180, .6790450928381962864721485411140583554377, \
        .38971675114002518602873692543653305619950, .6772486772486772486772486772486772486772, \
        .39235876060286384303665840889152605086580, .6754617414248021108179419525065963060686, \
        .39499380824086893770896722344332374632350, .6736842105263157894736842105263157894737, \
        .39762193064713846624158577469643205404280, .6719160104986876640419947506561679790026, \
        .40024316412701266276741307592601515352730, .6701570680628272251308900523560209424084, \
        .40285754470108348090917615991202183067800, .6684073107049608355091383812010443864230, \
        .40546510810816432934799991016916465014230, .6666666666666666666666666666666666666667, \
        .40806588980822172674223224930756259709600, .6649350649350649350649350649350649350649, \
        .41065992498526837639616360320360399782650, .6632124352331606217616580310880829015544, \
        .41324724855021932601317757871584035456180, .6614987080103359173126614987080103359173, \
        .41582789514371093497757669865677598863850, .6597938144329896907216494845360824742268, \
        .41840189913888381489925905043492093682300, .6580976863753213367609254498714652956298, \
        .42096929464412963239894338585145305842150, .6564102564102564102564102564102564102564, \
        .42353011550580327293502591601281892508280, .6547314578005115089514066496163682864450, \
        .42608439531090003260516141381231136620050, .6530612244897959183673469387755102040816, \
        .42863216738969872610098832410585600882780, .6513994910941475826972010178117048346056, \
        .43117346481837132143866142541810404509300, .6497461928934010152284263959390862944162, \
        .43370832042155937902094819946796633303180, .6481012658227848101265822784810126582278, \
        .43623676677491801667585491486534010618930, .6464646464646464646464646464646464646465, \
        .43875883620762790027214350629947148263450, .6448362720403022670025188916876574307305, \
        .44127456080487520440058801796112675219780, .6432160804020100502512562814070351758794, \
        .44378397241030093089975139264424797147500, .6416040100250626566416040100250626566416, \
        .44628710262841947420398014401143882423650, .6400000000000000000000000000000000000000, \
        .44878398282700665555822183705458883196130, .6384039900249376558603491271820448877805, \
        .45127464413945855836729492693848442286250, .6368159203980099502487562189054726368159, \
        .45375911746712049854579618113348260521900, .6352357320099255583126550868486352357320, \
        .45623743348158757315857769754074979573500, .6336633663366336633663366336633663366337, \
        .45870962262697662081833982483658473938700, .6320987654320987654320987654320987654321, \
        .46117571512217014895185229761409573256980, .6305418719211822660098522167487684729064, \
        .46363574096303250549055974261136725544930, .6289926289926289926289926289926289926290, \
        .46608972992459918316399125615134835243230, .6274509803921568627450980392156862745098, \
        .46853771156323925639597405279346276074650, .6259168704156479217603911980440097799511, \
        .47097971521879100631480241645476780831830, .6243902439024390243902439024390243902439, \
        .47341577001667212165614273544633761048330, .6228710462287104622871046228710462287105, \
        .47584590486996386493601107758877333253630, .6213592233009708737864077669902912621359, \
        .47827014848147025860569669930555392056700, .6198547215496368038740920096852300242131, \
        .48068852934575190261057286988943815231330, .6183574879227053140096618357487922705314, \
        .48310107575113581113157579238759353756900, .6168674698795180722891566265060240963855, \
        .48550781578170076890899053978500887751580, .6153846153846153846153846153846153846154, \
        .48790877731923892879351001283794175833480, .6139088729016786570743405275779376498801, \
        .49030398804519381705802061333088204264650, .6124401913875598086124401913875598086124, \
        .49269347544257524607047571407747454941280, .6109785202863961813842482100238663484487, \
        .49507726679785146739476431321236304938800, .6095238095238095238095238095238095238095, \
        .49745538920281889838648226032091770321130, .6080760095011876484560570071258907363420, \
        .49982786955644931126130359189119189977650, .6066350710900473933649289099526066350711, \
        .50219473456671548383667413872899487614650, .6052009456264775413711583924349881796690, \
        .50455601075239520092452494282042607665050, .6037735849056603773584905660377358490566, \
        .50691172444485432801997148999362252652650, .6023529411764705882352941176470588235294, \
        .50926190178980790257412536448100581765150, .6009389671361502347417840375586854460094, \
        .51160656874906207391973111953120678663250, .5995316159250585480093676814988290398126, \
        .51394575110223428282552049495279788970950, .5981308411214953271028037383177570093458, \
        .51627947444845445623684554448118433356300, .5967365967365967365967365967365967365967, \
        .51860776420804555186805373523384332656850, .5953488372093023255813953488372093023256, \
        .52093064562418522900344441950437612831600, .5939675174013921113689095127610208816705, \
        .52324814376454775732838697877014055848100, .5925925925925925925925925925925925925926, \
        .52556028352292727401362526507000438869000, .5912240184757505773672055427251732101617, \
        .52786708962084227803046587723656557500350, .5898617511520737327188940092165898617512, \
        .53016858660912158374145519701414741575700, .5885057471264367816091954022988505747126, \
        .53246479886947173376654518506256863474850, .5871559633027522935779816513761467889908, \
        .53475575061602764748158733709715306758900, .5858123569794050343249427917620137299771, \
        .53704146589688361856929077475797384977350, .5844748858447488584474885844748858447489, \
        .53932196859560876944783558428753167390800, .5831435079726651480637813211845102505695, \
        .54159728243274429804188230264117009937750, .5818181818181818181818181818181818181818, \
        .54386743096728351609669971367111429572100, .5804988662131519274376417233560090702948, \
        .54613243759813556721383065450936555862450, .5791855203619909502262443438914027149321, \
        .54839232556557315767520321969641372561450, .5778781038374717832957110609480812641084, \
        .55064711795266219063194057525834068655950, .5765765765765765765765765765765765765766, \
        .55289683768667763352766542084282264113450, .5752808988764044943820224719101123595506, \
        .55514150754050151093110798683483153581600, .5739910313901345291479820627802690582960, \
        .55738115013400635344709144192165695130850, .5727069351230425055928411633109619686801, \
        .55961578793542265941596269840374588966350, .5714285714285714285714285714285714285714, \
        .56184544326269181269140062795486301183700, .5701559020044543429844097995545657015590, \
        .56407013828480290218436721261241473257550, .5688888888888888888888888888888888888889, \
        .56628989502311577464155334382667206227800, .5676274944567627494456762749445676274945, \
        .56850473535266865532378233183408156037350, .5663716814159292035398230088495575221239, \
        .57071468100347144680739575051120482385150, .5651214128035320088300220750551876379691, \
        .57291975356178548306473885531886480748650, .5638766519823788546255506607929515418502, \
        .57511997447138785144460371157038025558000, .5626373626373626373626373626373626373626, \
        .57731536503482350219940144597785547375700, .5614035087719298245614035087719298245614, \
        .57950594641464214795689713355386629700650, .5601750547045951859956236323851203501094, \
        .58169173963462239562716149521293118596100, .5589519650655021834061135371179039301310, \
        .58387276558098266665552955601015128195300, .5577342047930283224400871459694989106754, \
        .58604904500357812846544902640744112432000, .5565217391304347826086956521739130434783, \
        .58822059851708596855957011939608491957200, .5553145336225596529284164859002169197397, \
        .59038744660217634674381770309992134571100, .5541125541125541125541125541125541125541, \
        .59254960960667157898740242671919986605650, .5529157667386609071274298056155507559395, \
        .59470710774669277576265358220553025603300, .5517241379310344827586206896551724137931, \
        .59685996110779382384237123915227130055450, .5505376344086021505376344086021505376344, \
        .59900818964608337768851242799428291618800, .5493562231759656652360515021459227467811, \
        .60115181318933474940990890900138765573500, .5481798715203426124197002141327623126338, \
        .60329085143808425240052883964381180703650, .5470085470085470085470085470085470085470, \
        .60542532396671688843525771517306566238400, .5458422174840085287846481876332622601279, \
        .60755525022454170969155029524699784815300, .5446808510638297872340425531914893617021, \
        .60968064953685519036241657886421307921400, .5435244161358811040339702760084925690021, \
        .61180154110599282990534675263916142284850, .5423728813559322033898305084745762711864, \
        .61391794401237043121710712512140162289150, .5412262156448202959830866807610993657505, \
        .61602987721551394351138242200249806046500, .5400843881856540084388185654008438818565, \
        .61813735955507864705538167982012964785100, .5389473684210526315789473684210526315789, \
        .62024040975185745772080281312810257077200, .5378151260504201680672268907563025210084, \
        .62233904640877868441606324267922900617100, .5366876310272536687631027253668763102725, \
        .62443328801189346144440150965237990021700, .5355648535564853556485355648535564853556, \
        .62652315293135274476554741340805776417250, .5344467640918580375782881002087682672234, \
        .62860865942237409420556559780379757285100, .5333333333333333333333333333333333333333, \
        .63068982562619868570408243613201193511500, .5322245322245322245322245322245322245322, \
        .63276666957103777644277897707070223987100, .5311203319502074688796680497925311203320, \
        .63483920917301017716738442686619237065300, .5300207039337474120082815734989648033126, \
        .63690746223706917739093569252872839570050, .5289256198347107438016528925619834710744, \
        .63897144645792069983514238629140891134750, .5278350515463917525773195876288659793814, \
        .64103117942093124081992527862894348800200, .5267489711934156378600823045267489711934, \
        .64308667860302726193566513757104985415950, .5256673511293634496919917864476386036961, \
        .64513796137358470073053240412264131009600, .5245901639344262295081967213114754098361, \
        .64718504499530948859131740391603671014300, .5235173824130879345603271983640081799591, \
        .64922794662510974195157587018911726772800, .5224489795918367346938775510204081632653, \
        .65126668331495807251485530287027359008800, .5213849287169042769857433808553971486762, \
        .65330127201274557080523663898929953575150, .5203252032520325203252032520325203252033, \
        .65533172956312757406749369692988693714150, .5192697768762677484787018255578093306288, \
        .65735807270835999727154330685152672231200, .5182186234817813765182186234817813765182, \
        .65938031808912778153342060249997302889800, .5171717171717171717171717171717171717172, \
        .66139848224536490484126716182800009846700, .5161290322580645161290322580645161290323, \
        .66341258161706617713093692145776003599150, .5150905432595573440643863179074446680080, \
        .66542263254509037562201001492212526500250, .5140562248995983935742971887550200803213, \
        .66742865127195616370414654738851822912700, .5130260521042084168336673346693386773547, \
        .66943065394262923906154583164607174694550, .5120000000000000000000000000000000000000, \
        .67142865660530226534774556057527661323550, .5109780439121756487025948103792415169661, \
        .67342267521216669923234121597488410770900, .5099601593625498007968127490039840637450, \
        .67541272562017662384192817626171745359900, .5089463220675944333996023856858846918489, \
        .67739882359180603188519853574689477682100, .5079365079365079365079365079365079365079, \
        .67938098479579733801614338517538271844400, .5069306930693069306930693069306930693069, \
        .68135922480790300781450241629499942064300, .5059288537549407114624505928853754940711, \
        .68333355911162063645036823800182901322850, .5049309664694280078895463510848126232742, \
        .68530400309891936760919861626462079584600, .5039370078740157480314960629921259842520, \
        .68727057207096020619019327568821609020250, .5029469548133595284872298624754420432220, \
        .68923328123880889251040571252815425395950, .5019607843137254901960784313725490196078, \
        .69314718055994530941723212145818,          5.0e-01,                                   \
    }

static constexpr float log_tab_32f[log_tab_size] = LOG_TAB_VALUE;
static constexpr double log_tab_64f[log_tab_size] = LOG_TAB_VALUE;

#undef LOG_TAB_VALUE

}  // namespace detail

int log32f(const float* src, float* dst, int _len)
{
    size_t vl = __riscv_vsetvlmax_e32m4();
    auto log_a2 = __riscv_vfmv_v_f_f32m4(detail::log32f_a2, vl);
    for (size_t len = _len; len > 0; len -= vl, src += vl, dst += vl)
    {
        vl = __riscv_vsetvl_e32m4(len);
        auto i0 = __riscv_vle32_v_i32m4((const int32_t*)src, vl);

        auto buf_i = __riscv_vor(__riscv_vand(i0, detail::log32f_mask, vl), 127 << 23, vl);
        auto idx = __riscv_vreinterpret_u32m4(__riscv_vand(
            __riscv_vsra(i0, 23 - detail::log_scale - 1 - 2, vl),
            detail::log_mask << 2,
            vl));

        auto tab_v = __riscv_vluxei32(detail::log_tab_32f, idx, vl);
        auto y0_i = __riscv_vsub(__riscv_vand(__riscv_vsra(i0, 23, vl), 0xff, vl), 127, vl);
        auto y0 = __riscv_vfmadd(__riscv_vfcvt_f_x_v_f32m4(y0_i, vl), detail::ln_2, tab_v, vl);

        tab_v = __riscv_vluxei32(detail::log_tab_32f, __riscv_vadd(idx, 4, vl), vl);
        auto buf_f = __riscv_vreinterpret_f32m4(buf_i);
        auto x0 = __riscv_vfmul(__riscv_vfsub(buf_f, 1.f, vl), tab_v, vl);
        x0 = __riscv_vfsub_mu(__riscv_vmseq(idx, (uint32_t)510 * 4, vl), x0, x0, 1.f / 512, vl);

        auto res = __riscv_vfadd(__riscv_vfmul(x0, detail::log32f_a0, vl), detail::log32f_a1, vl);
        res = __riscv_vfmadd(res, x0, log_a2, vl);
        res = __riscv_vfmadd(res, x0, y0, vl);

        __riscv_vse32(dst, res, vl);
    }

    return CV_HAL_ERROR_OK;
}

int log64f(const double* src, double* dst, int _len)
{
    size_t vl = __riscv_vsetvlmax_e64m4();
    // all vector registers are used up, so not load more constants
    auto log_a5 = __riscv_vfmv_v_f_f64m4(detail::log64f_a5, vl);
    auto log_a6 = __riscv_vfmv_v_f_f64m4(detail::log64f_a6, vl);
    auto log_a7 = __riscv_vfmv_v_f_f64m4(detail::log64f_a7, vl);
    for (size_t len = _len; len > 0; len -= vl, src += vl, dst += vl)
    {
        vl = __riscv_vsetvl_e64m4(len);
        auto i0 = __riscv_vle64_v_i64m4((const int64_t*)src, vl);

        auto buf_i = __riscv_vor(__riscv_vand(i0, detail::log64f_mask, vl), (size_t)1023 << 52, vl);
        auto idx = __riscv_vreinterpret_u64m4(__riscv_vand(
            __riscv_vsra(i0, 52 - detail::log_scale - 1 - 3, vl),
            detail::log_mask << 3,
            vl));

        auto tab_v = __riscv_vluxei64(detail::log_tab_64f, idx, vl);
        auto y0_i = __riscv_vsub(__riscv_vand(__riscv_vsra(i0, 52, vl), 0x7ff, vl), 1023, vl);
        auto y0 = __riscv_vfmadd(__riscv_vfcvt_f_x_v_f64m4(y0_i, vl), detail::ln_2, tab_v, vl);

        tab_v = __riscv_vluxei64(detail::log_tab_64f, __riscv_vadd(idx, 8, vl), vl);
        auto buf_f = __riscv_vreinterpret_f64m4(buf_i);
        auto x0 = __riscv_vfmul(__riscv_vfsub(buf_f, 1.0, vl), tab_v, vl);
        x0 = __riscv_vfsub_mu(__riscv_vmseq(idx, (uint64_t)510 * 8, vl), x0, x0, 1. / 512, vl);

        auto res = __riscv_vfadd(__riscv_vfmul(x0, detail::log64f_a0, vl), detail::log64f_a1, vl);
        res = __riscv_vfadd(__riscv_vfmul(x0, res, vl), detail::log64f_a2, vl);
        res = __riscv_vfadd(__riscv_vfmul(x0, res, vl), detail::log64f_a3, vl);
        res = __riscv_vfadd(__riscv_vfmul(x0, res, vl), detail::log64f_a4, vl);
        res = __riscv_vfmadd(res, x0, log_a5, vl);
        res = __riscv_vfmadd(res, x0, log_a6, vl);
        res = __riscv_vfmadd(res, x0, log_a7, vl);
        res = __riscv_vfmadd(res, x0, y0, vl);

        __riscv_vse64(dst, res, vl);
    }

    return CV_HAL_ERROR_OK;
}

#endif // CV_HAL_RVV_1P0_ENABLED

}}}  // cv::rvv_hal::core
