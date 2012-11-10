#include "loadcell.hpp"

#include <string>
#include <sstream>
#include <c++/4.6/list>

#include "esmreader.hpp"
#include "esmwriter.hpp"

namespace ESM
{

void CellRef::save(ESMWriter &esm)
{
    esm.writeHNT("FRMR", mRefnum);
    esm.writeHNCString("NAME", mRefID);

    if (mScale != 1.0) {
        esm.writeHNT("XSCL", mScale);
    }

    esm.writeHNOCString("ANAM", mOwner);
    esm.writeHNOCString("BNAM", mGlob);
    esm.writeHNOCString("XSOL", mSoul);

    esm.writeHNOCString("CNAM", mFaction);
    if (mFactIndex != -2) {
        esm.writeHNT("INDX", mFactIndex);
    }

    if (mCharge != -1.0) {
        esm.writeHNT("XCHG", mCharge);
    }

    if (mIntv != -1) {
        esm.writeHNT("INTV", mIntv);
    }
    if (mNam9 != 0) {
        esm.writeHNT("NAM9", mNam9);
    }

    if (mTeleport)
    {
        esm.writeHNT("DODT", mDoorDest);
        esm.writeHNOCString("DNAM", mDestCell);
    }

    if (mLockLevel != -1) {
        esm.writeHNT("FLTV", mLockLevel);
    }
    esm.writeHNOCString("KNAM", mKey);
    esm.writeHNOCString("TNAM", mTrap);

    if (mUnam != -1) {
        esm.writeHNT("UNAM", mUnam);
    }
    if (mFltv != 0) {
        esm.writeHNT("FLTV", mFltv);
    }

    esm.writeHNT("DATA", mPos, 24);
    if (mNam0 != 0) {
        esm.writeHNT("NAM0", mNam0);
    }
}

void Cell::load(ESMReader &esm)
{
    // Ignore this for now, it might mean we should delete the entire
    // cell?
    if (esm.isNextSub("DELE")) {
        esm.skipHSub();
    }

    esm.getHNT(mData, "DATA", 12);

    // Water level
    mWater = -1;
    mNAM0 = 0;

    if (mData.mFlags & Interior)
    {
        // Interior cells
        if (esm.isNextSub("INTV"))
        {
            int waterl;
            esm.getHT(waterl);
            mWater = (float) waterl;
            mWaterInt = true;
        }
        else if (esm.isNextSub("WHGT"))
            esm.getHT(mWater);

        // Quasi-exterior cells have a region (which determines the
        // weather), pure interior cells have ambient lighting
        // instead.
        if (mData.mFlags & QuasiEx)
            mRegion = esm.getHNOString("RGNN");
        else
            esm.getHNT(mAmbi, "AMBI", 16);
    }
    else
    {
        // Exterior cells
        mRegion = esm.getHNOString("RGNN");

        mMapColor = 0;
        esm.getHNOT(mMapColor, "NAM5");
    }
    if (esm.isNextSub("NAM0")) {
        esm.getHT(mNAM0);
    }

    // Save position of the cell references and move on
    mContextList.push_back(esm.getContext());
    if (mContextList.size() > 1)
        std::cout << "found two plugins" << std::endl;
    esm.skipRecord();
}

void Cell::save(ESMWriter &esm)
{
    esm.writeHNT("DATA", mData, 12);
    if (mData.mFlags & Interior)
    {
        if (mWater != -1) {
            if (mWaterInt) {
                int water =
                    (mWater >= 0) ? (int) (mWater + 0.5) : (int) (mWater - 0.5);
                esm.writeHNT("INTV", water);
            } else {
                esm.writeHNT("WHGT", mWater);
            }
        }

        if (mData.mFlags & QuasiEx)
            esm.writeHNOCString("RGNN", mRegion);
        else
            esm.writeHNT("AMBI", mAmbi, 16);
    }
    else
    {
        esm.writeHNOCString("RGNN", mRegion);
        if (mMapColor != 0)
            esm.writeHNT("NAM5", mMapColor);
    }
    
    if (mNAM0 != 0)
        esm.writeHNT("NAM0", mNAM0);
}

void Cell::restore(ESMReader &esm, int iCtx) const
{
    esm.restoreContext(mContextList[iCtx]);
}

std::string Cell::getDescription() const
{
    if (mData.mFlags & Interior)
    {
        return mName;
    }
    else
    {
        std::ostringstream stream;
        stream << mData.mX << ", " << mData.mY;
        return stream.str();
    }
}

bool Cell::getNextRef(ESMReader &esm, CellRef &ref)
{
    // TODO: Add support for multiple plugins. This requires a tricky renaming scheme for "ref.mRefnum".
    //  I'll probably add something to "ESMReader", we will need one per plugin anyway.
    // TODO: Try and document reference numbering, I don't think this has been done anywhere else.
    if (!esm.hasMoreSubs())
        return false;
    
    if (esm.isNextSub("MVRF")) {
        // Moved existing reference across cell boundaries, so interpret the blocks correctly.
        // FIXME: Right now, we don't do anything with this data. This might result in weird behaviour,
        //  where a moved reference does not appear because the owning cell (i.e. this cell) is not
        //  loaded in memory.
        int movedRefnum = 0;
        int destCell[2];
        esm.getHT(movedRefnum);
        esm.getHNT(destCell, "CNDT");
        // TODO: Figure out what happens when a reference has moved into an interior cell. This might
        //  be required for NPCs following the player.
    }
    // If we have just parsed a MVRF entry, there should be a regular FRMR entry following right there.
    // With the exception that this bock technically belongs to a different cell than this one.
    // TODO: Figure out a way to handle these weird references that do not belong to this cell.
    //  This may require some not-so-small behing-the-scenes updates.
    esm.getHNT(ref.mRefnum, "FRMR");
    ref.mRefID = esm.getHNString("NAME");

    // getHNOT will not change the existing value if the subrecord is
    // missing
    ref.mScale = 1.0;
    esm.getHNOT(ref.mScale, "XSCL");

    ref.mOwner = esm.getHNOString("ANAM");
    ref.mGlob = esm.getHNOString("BNAM");
    ref.mSoul = esm.getHNOString("XSOL");

    ref.mFaction = esm.getHNOString("CNAM");
    ref.mFactIndex = -2;
    esm.getHNOT(ref.mFactIndex, "INDX");

    ref.mCharge = -1.0;
    esm.getHNOT(ref.mCharge, "XCHG");

    ref.mIntv = -1;
    ref.mNam9 = 0;
    esm.getHNOT(ref.mIntv, "INTV");
    esm.getHNOT(ref.mNam9, "NAM9");

    // Present for doors that teleport you to another cell.
    if (esm.isNextSub("DODT"))
    {
        ref.mTeleport = true;
        esm.getHT(ref.mDoorDest);
        ref.mDestCell = esm.getHNOString("DNAM");
    } else {
        ref.mTeleport = false;
    }

    // Integer, despite the name suggesting otherwise
    ref.mLockLevel = -1;
    esm.getHNOT(ref.mLockLevel, "FLTV");
    ref.mKey = esm.getHNOString("KNAM");
    ref.mTrap = esm.getHNOString("TNAM");

    ref.mUnam = -1;
    ref.mFltv = 0;
    esm.getHNOT(ref.mUnam, "UNAM");
    esm.getHNOT(ref.mFltv, "FLTV");

    esm.getHNT(ref.mPos, "DATA", 24);

    // Number of references in the cell? Maximum once in each cell,
    // but not always at the beginning, and not always right. In other
    // words, completely useless.
    // Update: Well, maybe not completely useless. This might actually be
    //  number_of_references + number_of_references_moved_here_Across_boundaries,
    //  and could be helpful for collecting these weird moved references.
    ref.mNam0 = 0;
    if (esm.isNextSub("NAM0"))
    {
        esm.getHT(ref.mNam0);
        //esm.getHNOT(NAM0, "NAM0");
    }

    return true;
}

}
