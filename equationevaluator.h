#ifndef EQUATIONEVALUATOR_H
#define EQUATIONEVALUATOR_H

#include <map>
#include <string>
#include <vector>

class evaluator {
public:
    double evaluate(const std::string& expression);
    bool checkNumbersUsed(const std::string& expression, std::vector<int> numbers);

private:
    std::map<char, int> precedence = {
        {'+', 1}, {'-', 1}, {'*', 2}, {'/', 2}
    };

    bool isOperator(char c);
    std::vector<std::string> shunting_yard(const std::string& expression);
    double evaluate_rpn(const std::vector<std::string>& rpn);
};

#endif
