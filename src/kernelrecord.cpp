#include <consensus/consensus.h>
#include <interfaces/wallet.h>
#include <key_io.h>
#include <timedata.h>
#include <validation.h>

#include <chain.h>
#include <chainparams.h>
#include <stdint.h>
#include <math.h>

#include <kernelrecord.h>

#include <base58.h>

using namespace std;

bool KernelRecord::showTransaction()
{
    return true;
}

/*
 * Decompose CWallet transaction to model kernel records.
 */
vector<KernelRecord> KernelRecord::decomposeOutput(const COutPoint& output, const interfaces::WalletTxOut& out)
{
    vector<KernelRecord> parts;
    int64_t nTime = out.time;
    int64_t nValue = out.txout.nValue;
    uint256 hash = output.hash;
    int64_t nDayWeight = (min((GetAdjustedTime() - nTime), (int64_t)(Params().GetConsensus().nStakeMaxAge+Params().GetConsensus().nStakeMinAge)) - Params().GetConsensus().nStakeMinAge); // DayWeight * 86400, чтобы был
    CTxDestination address;
    std::string addrStr;
    ExtractDestination(out.txout.scriptPubKey, address);
    addrStr = EncodeDestination(address);
    uint64_t coinAge = max( (nValue * nDayWeight) / (COIN * 86400), (int64_t)0);

    parts.push_back(KernelRecord(hash, nTime, addrStr, nValue, coinAge));
    return parts;
}

std::string KernelRecord::getTxID()
{
    return hash.ToString();
}

int64_t KernelRecord::getAge() const
{
    return (GetAdjustedTime() - nTime) / 86400;
}

uint64_t KernelRecord::getCoinDay() const
{
    int64_t nWeight = GetAdjustedTime() - nTime - Params().GetConsensus().nStakeMinAge;
    if( nWeight <  0)
        return 0;
    nWeight = min(nWeight, (int64_t)Params().GetConsensus().nStakeMaxAge);
    uint64_t coinAge = (nValue * nWeight ) / (COIN * 86400);
    return coinAge;
}

int64_t KernelRecord::getPoSReward(int minutes)
{
    int64_t PoSReward;
    int64_t nWeight = GetAdjustedTime() - nTime + minutes * 60;
    PoSReward = GetProofOfStakeReward(chainActive.Tip()->nHeight, nValue, nWeight, Params().GetConsensus());
    return PoSReward;
}

double KernelRecord::getProbToMintStake(double difficulty, int timeOffset) const
{
    //double maxTarget = pow(static_cast<double>(2), 224);
    //double target = maxTarget / difficulty;
    //int dayWeight = (min((GetAdjustedTime() - nTime) + timeOffset, (int64_t)(nStakeMinAge+nStakeMaxAge)) - nStakeMinAge) / 86400;
    //uint64_t coinAge = max(nValue * dayWeight / COIN, (int64_t)0);
    //return target * coinAge / pow(static_cast<double>(2), 256);
    int64_t Weight = (min((GetAdjustedTime() - nTime) + timeOffset, (int64_t)(Params().GetConsensus().nStakeMinAge+Params().GetConsensus().nStakeMaxAge)) - Params().GetConsensus().nStakeMinAge);
    uint64_t coinAge = max(nValue * Weight / (COIN * 86400), (int64_t)0);
    return coinAge / (pow(static_cast<double>(2),32) * difficulty);
}

double KernelRecord::getProbToMintWithinNMinutes(double difficulty, int minutes)
{
    if(difficulty != prevDifficulty || minutes != prevMinutes)
    {
        double prob = 1;
        double p;
        int d = minutes / (60 * 24); // Number of full days
        int m = minutes % (60 * 24); // Number of minutes in the last day
        int i, timeOffset;

        // Probabilities for the first d days
        for(i = 0; i < d; i++)
        {
            timeOffset = i * 86400;
            p = pow(1 - getProbToMintStake(difficulty, timeOffset), 86400);
            prob *= p;
        }

        // Probability for the m minutes of the last day
        timeOffset = d * 86400;
        p = pow(1 - getProbToMintStake(difficulty, timeOffset), 60 * m);
        prob *= p;

        prob = 1 - prob;
        prevProbability = prob;
        prevDifficulty = difficulty;
        prevMinutes = minutes;
    }
    return prevProbability;
}
