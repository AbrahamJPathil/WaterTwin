import pandas as pd
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import StandardScaler
from sklearn.linear_model import LogisticRegression
from sklearn.tree import DecisionTreeClassifier
from sklearn.ensemble import RandomForestClassifier

X_train_scaled = pd.read_csv('preprocessed_data/X_train_scaled.csv')
X_test_scaled = pd.read_csv('preprocessed_data/X_test_scaled.csv')
y_train = pd.read_csv('preprocessed_data/y_train.csv')
y_test = pd.read_csv('preprocessed_data/y_test.csv')

MODELS = {
    "LogisticRegression": {
        "model": lambda **p: Pipeline(
            [("scale", StandardScaler()), ("lr", LogisticRegression(max_iter=2000, random_state=SEED, **p))]
        ),
        "params": {"C": [0.1, 1.0, 10.0]},
    },
    "DecisionTree": {
        "model": lambda **p: DecisionTreeClassifier(random_state=SEED, **p),
        "params": {"max_depth": [3, 4, 5, 6], "min_samples_leaf": [2, 3, 5]},
    },
    "RandomForest": {
        "model": lambda **p: RandomForestClassifier(n_jobs=-1, random_state=SEED, **p),
        "params": {
            "n_estimators": [50, 100],
            "max_depth": [3, 5, 7],
            "min_samples_leaf": [2, 5],
        },
    },
}
