/*
 * StackWithBonuses.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "StackWithBonuses.h"
#include "../../lib/NetPacksBase.h"
#include "../../lib/CStack.h"

void actualizeEffect(TBonusListPtr target, const Bonus & ef)
{
	for(auto bonus : *target) //TODO: optimize
	{
		if(bonus->source == Bonus::SPELL_EFFECT && bonus->type == ef.type && bonus->subtype == ef.subtype)
		{
			bonus->turnsRemain = std::max(bonus->turnsRemain, ef.turnsRemain);
		}
	}
}

StackWithBonuses::StackWithBonuses(const CStackState * Stack)
	: state(Stack->getUnitInfo(), this)
{
	state = *Stack;
}

const TBonusListPtr StackWithBonuses::getAllBonuses(const CSelector & selector, const CSelector & limit,
	const CBonusSystemNode * root, const std::string & cachingStr) const
{
	TBonusListPtr ret = std::make_shared<BonusList>();
	const TBonusListPtr originalList = state.unitAsBearer()->getAllBonuses(selector, limit, root, cachingStr);
	range::copy(*originalList, std::back_inserter(*ret));

	for(const Bonus & bonus : bonusesToUpdate)
	{
		if(selector(&bonus) && (!limit || !limit(&bonus)))
		{
			if(ret->getFirst(Selector::source(Bonus::SPELL_EFFECT, bonus.sid).And(Selector::typeSubtype(bonus.type, bonus.subtype))))
			{
				actualizeEffect(ret, bonus);
			}
			else
			{
				auto b = std::make_shared<Bonus>(bonus);
				ret->push_back(b);
			}
		}
	}

	for(auto & bonus : bonusesToAdd)
	{
		auto b = std::make_shared<Bonus>(bonus);
		if(selector(b.get()) && (!limit || !limit(b.get())))
			ret->push_back(b);
	}
	//TODO limiters?
	return ret;
}

const IBonusBearer * StackWithBonuses::unitAsBearer() const
{
	return this;
}

bool StackWithBonuses::unitHasAmmoCart() const
{
	//FIXME: check ammocart alive state here
	return false;
}


HypotheticBattle::HypotheticBattle(Subject realBattle)
	: BattleProxy(realBattle)
{

}

std::shared_ptr<StackWithBonuses> HypotheticBattle::getForUpdate(uint32_t id)
{
	auto iter = stackStates.find(id);

	if(iter == stackStates.end())
	{
		const CStack * s = subject->battleGetStackByID(id, false);

		auto ret = std::make_shared<StackWithBonuses>(&s->stackState);
		stackStates[id] = ret;
		return ret;
	}
	else
	{
		return iter->second;
	}
}

battle::Units HypotheticBattle::getUnitsIf(battle::UnitFilter predicate) const
{
	//TODO: added units (clones, summoned etc)

	auto getAll = [](const battle::Unit * unit)->bool
	{
		return !unit->isGhost();
	};

	battle::Units all = BattleProxy::getUnitsIf(getAll);

	battle::Units ret;

	for(auto one : all)
	{
		auto ID = one->unitId();

		auto iter = stackStates.find(ID);

		if(iter == stackStates.end())
		{
			if(predicate(one))
				ret.push_back(one);
		}
		else
		{
			auto & swb = iter->second;
			const battle::Unit * changed = &(swb->state);

			if(predicate(changed))
				ret.push_back(changed);
		}
	}

	return ret;
}


void HypotheticBattle::updateUnit(const CStackStateInfo & changes)
{
	std::shared_ptr<StackWithBonuses> changed = getForUpdate(changes.stackId);

	changed->state.fromInfo(changes);

	//todo: bonuses
}


