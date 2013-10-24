#include "VCStoreInMemory.h"
#include "vcookie.h"
#include <time.h>

#include "fct.h"

void SetupVCookie (VCookie &vc)
{
    time_t t = time(0);
    
    vc.SetFirstHitTimeGMT(t-100000);
    vc.SetLastHitTimeGMT(t);
    vc.SetLastHitTimeVisitorLocal(t - 7*60*60);
    vc.SetLastVisitNum(vc.GetLastVisitNum() + 1);

    vc.SetLastPurchaseTimeGMT(t-10000);
    vc.SetFirstHitReferrer("Some Site");
    vc.SetFirstHitUrl("http://www.acme.com/a/b/d/x.html");
    vc.SetFirstHitPagename("Some Page");
    vc.SetLastPurchaseNum(5);
    
    vc.SetMerchandising ( "asl;dfkjasd;flkjasdf;lkjasdf;lkajsdf;lkasjdf;laksdjf;alksdjfa;lksdjfa;lksdjfa;lsdkfja;lsdkfjas;ldkfjas;ldfkja;lsdkfja;sldkfjals;dkfjas;ldkfjasl;kdfjasl;kdfjas;ldkfjas;ldkfjasl;dkfja;sldkfjasl;kdjfal;skdjfasl;kdjfals;dkfjasl;dkfjasl;kdjqert;lkajsdg;laketn;aksdbnvxc;kgnasd;tkjnasd;gkjncb;akjsdnta;skdjgnb;lkdgjans;ckvjnad;ksfgajsd;flkgjvnc;kajsdgfna;skdfnv;kcxbm,vna;skedjtrhnasd;kgjvnxcz;kjgadnskgljbzncvlkjsmdng;kjdfnlkagsdmng;dkljcmnga;sdkjfnav;lckxj,mfgna;ldskgj;zldsvmbn;zxlkfmnfd;kglb,mnzx;flkjads,mnf;alksd,mgnvbc;kxj,fmasjdn;lkfjasnd;vkjnasfx;");
    
    vc.SetPurchaseId("pid1");
    vc.SetPurchaseId("pid2");
    vc.SetPurchaseId("pid3");
    vc.SetPurchaseId("pid4");
    vc.SetPurchaseId("pid5");
    vc.SetPurchaseId("pid6");
    
    vc.SetVar(1, "Var1", t+1, 1, ALLOC_TYPE_FIRST);
    vc.SetVar(9, "Var9", t+9, 9, ALLOC_TYPE_FIRST);
    vc.SetVar(5, "Var5", t+5, 5, ALLOC_TYPE_FIRST);
    vc.SetVar(7, "Var7", t+7, 7, ALLOC_TYPE_FIRST);
    vc.SetVar(3, "Var3", t+3, 3, ALLOC_TYPE_FIRST);
    vc.SetVar(8, "Var8", t+8, 8, ALLOC_TYPE_FIRST);
    vc.SetVar(4, "Var4", t+4, 4, ALLOC_TYPE_FIRST);
    vc.SetVar(2, "Var2", t+2, 2, ALLOC_TYPE_FIRST);
    vc.SetVar(6, "Var6", t+6, 6, ALLOC_TYPE_FIRST);
    vc.SetVar(10, "Var10", t+10, 10, ALLOC_TYPE_FIRST);
    vc.SetVar(13, "Var13", t+13, 13, ALLOC_TYPE_FIRST);
    vc.SetVar(12, "Var12", t+12, 12, ALLOC_TYPE_FIRST);
    vc.SetVar(11, "Var11", t+11, 11, ALLOC_TYPE_FIRST);
    
    vc.ClearVar (11);
    
    vc.SetVar (9, "Var9b", t+99, 19, ALLOC_TYPE_LINEAR, 5);
    vc.SetVar (9, "Var9c", t+999, 29, ALLOC_TYPE_LINEAR, 5);
    vc.SetVar (9, "Var9d", t+9999, 39, ALLOC_TYPE_LINEAR, 5);
    vc.SetVar (9, "Var9e", t+99999, 49, ALLOC_TYPE_LINEAR, 5);
    vc.SetVar (9, "Var9f", t+999999, 59, ALLOC_TYPE_LINEAR, 5);
    vc.SetVar (9, "Var9g", t+9999999, 69, ALLOC_TYPE_LINEAR, 5);
    vc.SetVar (9, "Var9h", t+99999999, 79, ALLOC_TYPE_LINEAR, 5);
}


#define CHK(a) if (a) ; else return false;

bool CheckVar (VCookie const &vc, VCookie::RelationId rid, std::string value, time_t t, unsigned char revision, char start, unsigned count=1)
{
    VCookie::VarId vid;
    CHK (vc.GetVarElementCount(rid, &vid) == count);
    for (unsigned i=0; i < count; ++i) {
        const VCookie::RelVar *rv = vc.GetVar (vid, i);
        
        CHK (rv != 0);
        static char d[2] = "0";
        d[0] = start + char(i);
        CHK (rv->value == value + d);
        CHK (rv->revision == revision+i);
        CHK (rv->timestamp == t+i);
    }
    return true;
}

FCT_BGN()
{
    {
        VCookieStore *pvcs = 0;


        FCT_FIXTURE_SUITE_BGN(VCookies)
        {
            
            
            FCT_SETUP_BGN()
            {
                pvcs = new VCStoreInMemory();
            }
            FCT_SETUP_END();
            
            FCT_TEARDOWN_BGN()
            {
                delete pvcs;
            }
            FCT_TEARDOWN_END();
            
            FCT_TEST_BGN(CreateVCookie)
            {
                VCookie vc(12345, 6789, 9876, true, *pvcs);
                
                fct_chk ( !vc.IsModified());
            }
            FCT_TEST_END();
            
            FCT_TEST_BGN(CreateVCookie2)
            {
                VCookie vc(12345, 6789, 9876, true, *pvcs);
                
                SetupVCookie (vc);
                
                fct_chk ( vc.IsModified());
            }
            FCT_TEST_END();
            
            FCT_TEST_BGN(TestRelationVars)
            {
                VCookie vc(12345, 6789, 9876, true, *pvcs);
                
                time_t t = time(0);
                vc.SetVar(1, "Var1a", t, 0, ALLOC_TYPE_FIRST);
                fct_chk (CheckVar (vc, 1, "Var1", t, 0, 'a'));
                
                vc.SetVar(1, "Var1a", t+1, 1,ALLOC_TYPE_FIRST);
                fct_chk (CheckVar (vc, 1, "Var1", t, 0, 'a'));

                vc.SetVar(1, "Var1b", t+2, 2, ALLOC_TYPE_LAST);
                fct_chk (CheckVar (vc, 1, "Var1", t+2, 2, 'b'));
                fct_chk (vc.GetVar(1,1) == 0);
                
                vc.ClearVar (1);
                fct_chk (vc.GetVarElementCount(1) == 0);
                fct_chk (vc.GetVar(1) == 0);
                
                vc.SetVar(1, "Var1c", t+3, 3, ALLOC_TYPE_FIRST);
                fct_chk (CheckVar (vc, 1, "Var1", t+3, 3, 'c'));
                
                vc.SetVar(1, "Var1d", t+4, 4, ALLOC_TYPE_LINEAR, 1);
                fct_chk (CheckVar (vc, 1, "Var1", t+4, 4, 'd'));
                
                vc.SetVar(1, "Var1e", t+5, 5, ALLOC_TYPE_LINEAR, 2);
                fct_chk (CheckVar (vc, 1, "Var1", t+4, 4, 'd', 2));
                         
                vc.SetVar(1, "Var1f", t+6, 6, ALLOC_TYPE_LINEAR, 3);
                fct_chk (CheckVar (vc, 1, "Var1", t+4, 4, 'd', 3));

                vc.SetVar(1, "Var1g", t+7, 7, ALLOC_TYPE_LINEAR, 3);
                fct_chk (CheckVar (vc, 1, "Var1", t+5, 5, 'e', 3));

                vc.SetVar(1, "Var1h", t+8, 8, ALLOC_TYPE_LINEAR, 2);
                fct_chk (CheckVar (vc, 1, "Var1", t+7, 7, 'g', 2));
            }
            FCT_TEST_END();
            
            
            FCT_TEST_BGN(LoadVCookie)
            {
                VCookie vc(12345, 6789, 9876, true, *pvcs);
                
                SetupVCookie (vc);
                
                pvcs->SaveVCookie(vc);
                
                VCookie vc2(12345, 6789, 9876, false, *pvcs);
                fct_chk ( !vc2.IsModified());
                
                fct_chk (vc == vc2);
            }
            FCT_TEST_END();
            
            FCT_TEST_BGN(DeleteOld)
            {
                for (unsigned i=0; i < 100; ++i) {
                    VCookie vc(12345, 6789+i, 9876-i, true, *pvcs);
                    vc.SetLastHitTimeGMT(100000*i+1+10000000*(i%2));
                }
                fct_chk (pvcs->GetVCookieCount() == 100);
                fct_chk (pvcs->DeleteOldVCookies(100000*50) == 25);
                fct_chk (pvcs->GetVCookieCount() == 75);
            }
            FCT_TEST_END();
        }
        FCT_FIXTURE_SUITE_END();
    }
}
FCT_END();
