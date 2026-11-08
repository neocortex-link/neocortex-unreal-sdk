#pragma once
#include "CoreMinimal.h"
#include "IDetailCustomization.h"

/**
 * Replaces the character and question set id fields of the Neocortex components with a text
 * field plus a picker listing what the project's API key can use.
 */
class FNeocortexIdCustomization : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance() { return MakeShared<FNeocortexIdCustomization>(); }
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
