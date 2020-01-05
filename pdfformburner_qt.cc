#include <QtCore/QCoreApplication>
#include <QtCore/QCommandLineParser>
#include <QtCore/QDateTime>
#include <iostream>
#include <poppler-qt5.h>
#include <poppler-form.h>
#include <memory>
#include <yaml-cpp/yaml.h>


#define BEGIN_ENUM(NS, TYPE) \
std::ostream & operator << (std::ostream & os, NS::TYPE value) {\
	typedef NS host; \
	switch(value) {
#define ENUM_VALUE(VAL) case(host::VAL): return os << #VAL;
#define END_ENUM \
	default: return os << "Invalid"; \
	}\
}

BEGIN_ENUM(Poppler::FormField, FormType)
	ENUM_VALUE(FormButton)
	ENUM_VALUE(FormText)
	ENUM_VALUE(FormChoice)
	ENUM_VALUE(FormSignature)
END_ENUM

BEGIN_ENUM(Poppler::FormFieldButton, ButtonType)
	ENUM_VALUE(Push)
	ENUM_VALUE(CheckBox)
	ENUM_VALUE(Radio)
END_ENUM

BEGIN_ENUM(Poppler::FormFieldText, TextType)
	ENUM_VALUE(Normal)
	ENUM_VALUE(Multiline)
	ENUM_VALUE(FileSelect)
END_ENUM
  
BEGIN_ENUM(Poppler::SignatureValidationInfo, SignatureStatus)
	ENUM_VALUE(SignatureValid)
	ENUM_VALUE(SignatureInvalid)
	ENUM_VALUE(SignatureDigestMismatch)
	ENUM_VALUE(SignatureDecodingError)
	ENUM_VALUE(SignatureGenericError)
	ENUM_VALUE(SignatureNotFound)
	ENUM_VALUE(SignatureNotVerified)
END_ENUM

QString translate(const char * text) {
	QCoreApplication & app = *QCoreApplication::instance();
	return app.translate("main", text);
}

void dump(Poppler::FormFieldButton * field, YAML::Emitter & out) {
	switch (field->buttonType()) {
		case Poppler::FormFieldButton::CheckBox:
		case Poppler::FormFieldButton::Radio:
			out << bool(field->state());
			return;
		case Poppler::FormFieldButton::Push:
			std::cerr
				<< "Push button ignored" << std::endl;
	}
}
void dump(Poppler::FormFieldText * field, YAML::Emitter & out) {
	switch (field->textType()) {
		case Poppler::FormFieldText::FileSelect:
			std::cerr
				<< "Warning: File select not fully supported, managed as simple text"
				<< std::endl;
			out << YAML::Value << field->text().toStdString();
			return;
		case Poppler::FormFieldText::Multiline:
			out << YAML::Literal;
		case Poppler::FormFieldText::Normal:
			out << YAML::Value << field->text().toStdString();
			return;
	}
}

void dump(Poppler::FormFieldChoice * field, YAML::Emitter & out) {
	auto choices = field->choices();
	auto currentChoices = field->currentChoices();
	if (field->multiSelect()) {
		out << YAML::BeginSeq;
		for (auto i: currentChoices) {
			out << YAML::Value << choices[i].toStdString();
		}
		out << YAML::EndSeq;
	}
	else if (field->isEditable()) {
		out << YAML::Value << field->editChoice().toStdString();
	}
	else {
		out << YAML::Value
			<< (currentChoices.empty()?"":choices[currentChoices.first()].toStdString());
	}
	out << YAML::Comment(translate("Choose %1 %2%3")
		.arg(field->multiSelect()?"some of ":"one of ")
		.arg(choices.join(", "))
		.arg(field->isEditable()?" or write your own":"")
		.toStdString());
}

void dump(Poppler::FormFieldSignature * field, YAML::Emitter & out) {
	auto info = field->validate(
		Poppler::FormFieldSignature::ValidateVerifyCertificate);
	out << YAML::BeginMap;
	out
		<< YAML::Key << "status"
		<< YAML::Value << info.signatureStatus()
		<< YAML::Key << "signer"
		<< YAML::Value << info.signerName().toStdString()
		<< YAML::Key << "time"
		<< YAML::Value << QDateTime::fromMSecsSinceEpoch(
				info.signingTime(), Qt::UTC)
			.toString(Qt::ISODate).toStdString()
		<< YAML::Key << "location"
		<< YAML::Value << info.location().toStdString()
		<< YAML::Key << "reason"
		<< YAML::Value << info.reason().toStdString()
		<< YAML::Key << "scope"
		<< YAML::Value << (info.signsTotalDocument()?"Total":"Partial")
	;
	out << YAML::EndMap;
}


class FieldTree {
public:
	FieldTree(Poppler::FormField * field=nullptr) : _field(field) {}
	void add(const QString & fullName, Poppler::FormField * field) {
		int dotPos = fullName.indexOf('.');
		if (dotPos == -1) {
			_children[fullName] = FieldTree(field);
			return;
		}
		QString levelName = fullName.left(dotPos);
		QString remaining = fullName.mid(dotPos+1);
		if (!_children.contains(levelName)) {
			_children[levelName] = FieldTree();
		}
		FieldTree & level = _children[levelName];
		level.add(remaining, field);
	}
	void extract(YAML::Emitter & out) {
		if (_field) extractField(out);
		else extractChildren(out);
	}

	void extractChildren(YAML::Emitter & out) {
		if (_children.empty()) return;
		out << YAML::BeginMap;
		for (auto key : _children.keys()) {
			out << YAML::Key << key.toStdString();
			_children[key].extract(out);
		}
		out << YAML::EndMap;
	}

	void extractField(YAML::Emitter & out) {
		switch (_field->type()) {
			case Poppler::FormField::FormButton:
				dump(dynamic_cast<Poppler::FormFieldButton*>(_field), out);
				break;
			case Poppler::FormField::FormText:
				dump(dynamic_cast<Poppler::FormFieldText*>(_field), out);
				break;
			case Poppler::FormField::FormChoice:
				dump(dynamic_cast<Poppler::FormFieldChoice*>(_field), out);
				break;
			case Poppler::FormField::FormSignature:
				dump(dynamic_cast<Poppler::FormFieldSignature*>(_field), out);
				break;
		}
		QString comment = translate("%1%2")
			.arg(_field->name()!=_field->uiName()?_field->uiName():QString())
			.arg(_field->isReadOnly()?" [Read Only]":"")
			;
		if (!comment.isEmpty())
			out << YAML::Comment(comment.toStdString());
	}
private:
	QMap<QString, FieldTree> _children;
	Poppler::FormField * _field;
};

int extractYamlFromPdf(FieldTree & fields, std::ostream & outputfile)
{
	YAML::Emitter out(outputfile);
	out << YAML::Comment("Generated by pdf-form-burner");
	fields.extract(out);
	return 0;
}


int main(int argc, char**argv)
{
	QCoreApplication app(argc, argv);
	app.setApplicationName("pdfformburner");
	app.setOrganizationName("KEEPerians UNLTD");
	app.setOrganizationDomain("kkep.org");
	app.setApplicationVersion("0.7");
	QCommandLineParser parser;
	parser.setApplicationDescription(translate("Extracts and fills PDF form data"));
	parser.addHelpOption();
	parser.addVersionOption();
	parser.addPositionalArgument("inputpdf", translate("Input PDF"));
	parser.addPositionalArgument("yaml", translate("YAML file with the form data"));
	parser.addPositionalArgument("outputpdf", translate("If provided uses the YAML as input and fills a pdf"), "[outputpdf]");
	parser.process(app);
	auto arguments = parser.positionalArguments();

	if (arguments.length()<1) {
		parser.showHelp(-1);
	}
	auto inputpdf = arguments[0];

	auto document = std::unique_ptr<Poppler::Document>(Poppler::Document::load(inputpdf));
	if (!document) {
		std::cerr << "Error: Unable to open the document" << std::endl;
		return -1;
	}
	if (document->isLocked()) {
		std::cerr << "Error: Locked pdf" << std::endl;
		return -1;
	}

	FieldTree fieldTree;
	for (unsigned page=0; true; page++) {
		Poppler::Page* pdfPage = document->page(page);  // Document starts at page 0
		if (pdfPage == 0) break;
		auto fields = pdfPage->formFields();
		for (auto field : fields) {
			fieldTree.add(field->fullyQualifiedName(), field);
		}
	}
	if (arguments.length()<2) {
		extractYamlFromPdf(fieldTree, std::cout);
	}

	return 0;
}
